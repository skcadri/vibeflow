#include "TranscriptionServer.h"
#include "Transcriber.h"

#include <QVector>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <sstream>

TranscriptionServer::TranscriptionServer(Transcriber *transcriber, QObject *parent)
    : QObject(parent)
    , m_transcriber(transcriber)
{
}

TranscriptionServer::~TranscriptionServer()
{
    stop();
}

void TranscriptionServer::start(int port)
{
    if (m_running.load())
        return;

    m_port = port;

    // --- GET /health ---
    m_server.Get("/health", [this](const httplib::Request &, httplib::Response &res) {
        res.set_header("Content-Type", "application/json");
        if (m_transcriber->isLoaded()) {
            res.set_content("{\"status\":\"ok\"}", "application/json");
        } else {
            res.set_content("{\"status\":\"loading\"}", "application/json");
        }
    });

    // --- POST /inference ---
    m_server.Post("/inference", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Content-Type", "application/json");

        if (!m_transcriber->isLoaded()) {
            fprintf(stderr, "[WARN] TranscriptionServer: model not loaded, returning 503\n");
            res.status = 503;
            res.set_content("{\"error\":\"model not loaded\"}", "application/json");
            return;
        }

        // Get uploaded file
        if (!req.has_file("file")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing 'file' field\"}", "application/json");
            return;
        }

        const auto &file = req.get_file_value("file");
        if (file.content.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"empty file\"}", "application/json");
            return;
        }

        // Parse WAV
        std::string parseError;
        auto wav = parseWav(file.content, parseError);
        if (!wav) {
            fprintf(stderr, "[ERROR] TranscriptionServer: invalid WAV: %s\n", parseError.c_str());
            res.status = 400;
            std::string body = "{\"error\":\"" + escapeJsonString(parseError) + "\"}";
            res.set_content(body, "application/json");
            return;
        }

        // Resample to 16kHz if needed
        std::vector<float> audio;
        if (wav->sampleRate != 16000) {
            audio = resampleTo16k(wav->samples, wav->sampleRate);
        } else {
            audio = std::move(wav->samples);
        }

        float durationSec = audio.size() / 16000.0f;
        fprintf(stderr, "[INFO] TranscriptionServer: POST /inference — %zu bytes, %.1fs audio\n",
                file.content.size(), durationSec);

        const int samplesPerChunk = 16000 * 30; // 30 seconds

        if ((int)audio.size() <= samplesPerChunk) {
            // Short audio — synchronous transcription
            QVector<float> qaudio(audio.begin(), audio.end());
            auto result = m_transcriber->transcribeWithSegments(qaudio, 16000);

            std::string body = "{\"text\":\"" + escapeJsonString(result.text) + "\","
                             + "\"segments\":" + segmentsToJson(result.segments) + "}";
            res.set_content(body, "application/json");
        } else {
            // Long audio — check one-job-at-a-time
            bool expected = false;
            if (!m_jobRunning.compare_exchange_strong(expected, true)) {
                res.status = 409;
                res.set_content("{\"error\":\"a job is already running\"}", "application/json");
                return;
            }

            // Create job
            auto job = std::make_shared<TranscriptionJob>();
            {
                std::lock_guard<std::mutex> lock(m_jobsMutex);
                job->id = std::to_string(m_nextJobId++);
                job->status = "processing";
                m_jobs[job->id] = job;
            }

            int totalChunks = ((int)audio.size() + samplesPerChunk - 1) / samplesPerChunk;
            fprintf(stderr, "[INFO] TranscriptionServer: job %s created — %d chunks\n",
                    job->id.c_str(), totalChunks);

            // Spawn worker thread
            std::thread worker([this, job, audio = std::move(audio)]() {
                transcribeChunked(job, std::move(audio));
            });
            worker.detach();

            res.status = 202;
            std::string body = "{\"job_id\":\"" + job->id + "\",\"status\":\"processing\"}";
            res.set_content(body, "application/json");
        }
    });

    // --- GET /jobs/:id ---
    m_server.Get(R"(/jobs/(\w+))", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Content-Type", "application/json");

        std::string jobId = req.matches[1];

        std::shared_ptr<TranscriptionJob> job;
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            auto it = m_jobs.find(jobId);
            if (it == m_jobs.end()) {
                res.status = 404;
                res.set_content("{\"error\":\"job not found\"}", "application/json");
                return;
            }
            job = it->second;
        }

        std::lock_guard<std::mutex> lock(job->mutex);

        std::string body = "{\"job_id\":\"" + job->id + "\","
                         + "\"status\":\"" + job->status + "\","
                         + "\"progress\":" + std::to_string(job->progress);

        if (job->status == "processing") {
            body += ",\"segments_completed\":" + std::to_string(job->segments.size());
            body += ",\"text_so_far\":\"" + escapeJsonString(job->fullText) + "\"";
        } else if (job->status == "complete") {
            body += ",\"text\":\"" + escapeJsonString(job->fullText) + "\"";
            body += ",\"segments\":" + segmentsToJson(job->segments);
        } else if (job->status == "failed") {
            body += ",\"error\":\"" + escapeJsonString(job->error) + "\"";
        }

        body += "}";
        res.set_content(body, "application/json");
    });

    // Start server in background thread
    m_serverThread = std::thread([this]() {
        if (!m_server.listen("127.0.0.1", m_port)) {
            fprintf(stderr, "[ERROR] TranscriptionServer: failed to bind port %d\n", m_port);
        }
    });

    m_running.store(true);
    fprintf(stderr, "[INFO] TranscriptionServer: started on 127.0.0.1:%d\n", m_port);
}

void TranscriptionServer::stop()
{
    if (!m_running.load())
        return;

    m_server.stop();
    if (m_serverThread.joinable())
        m_serverThread.join();
    m_running.store(false);

    fprintf(stderr, "[INFO] TranscriptionServer: stopped\n");
}

bool TranscriptionServer::isRunning() const
{
    return m_running.load();
}

// ─── WAV Parser ─────────────────────────────────────────────────────

std::optional<TranscriptionServer::WavData> TranscriptionServer::parseWav(
    const std::string &data, std::string &error)
{
    if (data.size() < 44) {
        error = "file too small to be a WAV";
        return std::nullopt;
    }

    const auto *d = reinterpret_cast<const uint8_t *>(data.data());

    // RIFF header
    if (memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) {
        error = "not a WAV file (missing RIFF/WAVE header)";
        return std::nullopt;
    }

    // Find fmt chunk
    size_t pos = 12;
    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
    bool foundFmt = false;

    while (pos + 8 <= data.size()) {
        uint32_t chunkSize;
        memcpy(&chunkSize, d + pos + 4, 4);

        if (memcmp(d + pos, "fmt ", 4) == 0) {
            if (pos + 8 + 16 > data.size()) {
                error = "truncated fmt chunk";
                return std::nullopt;
            }
            memcpy(&audioFormat, d + pos + 8, 2);
            memcpy(&numChannels, d + pos + 10, 2);
            memcpy(&sampleRate, d + pos + 12, 4);
            memcpy(&bitsPerSample, d + pos + 22, 2);
            foundFmt = true;
        }

        if (memcmp(d + pos, "data", 4) == 0) {
            if (!foundFmt) {
                error = "data chunk before fmt chunk";
                return std::nullopt;
            }

            // Validate format
            if (audioFormat != 1 && audioFormat != 3) { // 1=PCM, 3=IEEE float
                error = "unsupported audio format (only PCM and IEEE float)";
                return std::nullopt;
            }
            if (numChannels == 0) {
                error = "zero channels";
                return std::nullopt;
            }

            size_t dataStart = pos + 8;
            size_t dataLen = std::min((size_t)chunkSize, data.size() - dataStart);
            int bytesPerSample = bitsPerSample / 8;
            int frameSize = bytesPerSample * numChannels;

            if (frameSize == 0) {
                error = "invalid frame size";
                return std::nullopt;
            }

            size_t numFrames = dataLen / frameSize;
            if (numFrames == 0) {
                error = "no audio frames in data chunk";
                return std::nullopt;
            }

            WavData wav;
            wav.sampleRate = sampleRate;
            wav.samples.resize(numFrames);

            const uint8_t *sampleData = d + dataStart;

            for (size_t f = 0; f < numFrames; f++) {
                // Mix to mono: pick channel with max amplitude
                float best = 0.0f;
                float bestAbs = 0.0f;

                for (int ch = 0; ch < numChannels; ch++) {
                    float val = 0.0f;
                    const uint8_t *p = sampleData + f * frameSize + ch * bytesPerSample;

                    if (audioFormat == 1 && bitsPerSample == 16) {
                        int16_t s;
                        memcpy(&s, p, 2);
                        val = s / 32768.0f;
                    } else if (audioFormat == 3 && bitsPerSample == 32) {
                        memcpy(&val, p, 4);
                    } else {
                        error = "unsupported bit depth: " + std::to_string(bitsPerSample);
                        return std::nullopt;
                    }

                    float absVal = std::fabs(val);
                    if (absVal > bestAbs) {
                        bestAbs = absVal;
                        best = val;
                    }
                }

                wav.samples[f] = best;
            }

            return wav;
        }

        // Advance to next chunk (chunks are 2-byte aligned)
        pos += 8 + chunkSize;
        if (chunkSize % 2 != 0) pos++;
    }

    error = "no data chunk found in WAV file";
    return std::nullopt;
}

// ─── Resampler ──────────────────────────────────────────────────────

std::vector<float> TranscriptionServer::resampleTo16k(const std::vector<float> &in, int srcRate)
{
    if (srcRate == 16000 || in.empty())
        return in;

    double ratio = 16000.0 / srcRate;
    size_t outLen = (size_t)(in.size() * ratio);
    std::vector<float> out(outLen);

    for (size_t i = 0; i < outLen; i++) {
        double srcPos = i / ratio;
        size_t idx = (size_t)srcPos;
        double frac = srcPos - idx;

        if (idx + 1 < in.size()) {
            out[i] = (float)(in[idx] * (1.0 - frac) + in[idx + 1] * frac);
        } else {
            out[i] = in[std::min(idx, in.size() - 1)];
        }
    }

    return out;
}

// ─── JSON Helpers ───────────────────────────────────────────────────

std::string TranscriptionServer::escapeJsonString(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string TranscriptionServer::segmentsToJson(const std::vector<TranscriptionSegment> &segments)
{
    std::string json = "[";
    for (size_t i = 0; i < segments.size(); i++) {
        if (i > 0) json += ",";
        const auto &seg = segments[i];
        char startBuf[32], endBuf[32];
        snprintf(startBuf, sizeof(startBuf), "%.2f", seg.start);
        snprintf(endBuf, sizeof(endBuf), "%.2f", seg.end);
        json += "{\"start\":" + std::string(startBuf)
              + ",\"end\":" + std::string(endBuf)
              + ",\"text\":\"" + escapeJsonString(seg.text) + "\""
              + ",\"speaker_turn_next\":" + (seg.speakerTurnNext ? "true" : "false")
              + "}";
    }
    json += "]";
    return json;
}

// ─── Chunked Transcription ──────────────────────────────────────────
//
//   ┌────────┐  ┌────────┐  ┌────────┐  ┌────┐
//   │ chunk 1│  │ chunk 2│  │ chunk 3│  │ ...│
//   └───┬────┘  └───┬────┘  └───┬────┘  └──┬─┘
//       │           │           │           │
//       ▼           ▼           ▼           ▼
//   [lock mutex] [lock mutex] [lock mutex] [lock mutex]
//   whisper_full whisper_full whisper_full  ...
//   [unlock]     [unlock]     [unlock]     [unlock]
//       ↑                                     │
//       └─── hotkey can acquire mutex here ───┘
//

void TranscriptionServer::transcribeChunked(
    std::shared_ptr<TranscriptionJob> job,
    std::vector<float> audio)
{
    const int sampleRate = 16000;
    const int samplesPerChunk = sampleRate * 30; // 30 seconds
    int totalChunks = ((int)audio.size() + samplesPerChunk - 1) / samplesPerChunk;

    QString contextPrompt; // carry-forward: last ~50 words from previous chunk
    float timeOffset = 0.0f;

    for (int c = 0; c < totalChunks; c++) {
        int chunkStart = c * samplesPerChunk;
        int chunkEnd = std::min(chunkStart + samplesPerChunk, (int)audio.size());
        int chunkLen = chunkEnd - chunkStart;

        QVector<float> chunkAudio(audio.begin() + chunkStart, audio.begin() + chunkEnd);

        // transcribeWithSegments() handles its own mutex internally.
        // Between chunks the mutex is released, allowing hotkey transcriptions to interleave.
        TranscriptionResult chunkResult = m_transcriber->transcribeWithSegments(
            chunkAudio, sampleRate, contextPrompt);

        if (chunkResult.text.empty() && !m_transcriber->isLoaded()) {
            std::lock_guard<std::mutex> jlock(job->mutex);
            job->status = "failed";
            job->error = "model was unloaded during transcription";
            m_jobRunning.store(false);
            fprintf(stderr, "[ERROR] TranscriptionServer: job %s failed — model unloaded\n",
                    job->id.c_str());
            return;
        }

        // Adjust segment timestamps by chunk offset
        for (auto &seg : chunkResult.segments) {
            seg.start += timeOffset;
            seg.end += timeOffset;
        }

        timeOffset += chunkLen / (float)sampleRate;

        // Context carry-forward: extract last ~50 words for next chunk's initial_prompt
        if (!chunkResult.text.empty()) {
            std::istringstream iss(chunkResult.text);
            std::vector<std::string> words;
            std::string word;
            while (iss >> word) words.push_back(word);

            size_t start = words.size() > 50 ? words.size() - 50 : 0;
            std::string prompt;
            for (size_t i = start; i < words.size(); i++) {
                if (!prompt.empty()) prompt += " ";
                prompt += words[i];
            }
            contextPrompt = QString::fromStdString(prompt);
        }

        // Update job progress
        {
            std::lock_guard<std::mutex> jlock(job->mutex);
            job->segments.insert(job->segments.end(),
                                chunkResult.segments.begin(),
                                chunkResult.segments.end());
            if (!job->fullText.empty() && !chunkResult.text.empty())
                job->fullText += " ";
            job->fullText += chunkResult.text;
            job->progress = (int)((c + 1) * 100 / totalChunks);
        }

        fprintf(stderr, "[INFO] TranscriptionServer: job %s chunk %d/%d complete\n",
                job->id.c_str(), c + 1, totalChunks);
    }

    // Mark complete
    {
        std::lock_guard<std::mutex> jlock(job->mutex);
        job->status = "complete";
        job->progress = 100;
    }
    m_jobRunning.store(false);

    fprintf(stderr, "[INFO] TranscriptionServer: job %s complete — %zu segments\n",
            job->id.c_str(), job->segments.size());
}
