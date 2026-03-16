#pragma once

#include <QObject>

#include <httplib.h>

#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <optional>

class Transcriber;

struct TranscriptionSegment;

struct TranscriptionJob {
    std::string id;
    std::string status;       // "processing", "complete", "failed"
    int progress = 0;         // 0-100
    std::string error;
    std::string fullText;
    std::vector<TranscriptionSegment> segments;
    std::mutex mutex;         // protects this job's fields
};

class TranscriptionServer : public QObject
{
    Q_OBJECT
public:
    explicit TranscriptionServer(Transcriber *transcriber, QObject *parent = nullptr);
    ~TranscriptionServer();

    void start(int port = 8080);
    void stop();
    bool isRunning() const;

private:
    // WAV parsing
    struct WavData {
        std::vector<float> samples;
        int sampleRate;
    };
    static std::optional<WavData> parseWav(const std::string &data, std::string &error);

    // Resampling
    static std::vector<float> resampleTo16k(const std::vector<float> &in, int srcRate);

    // JSON helpers
    static std::string escapeJsonString(const std::string &s);
    static std::string segmentsToJson(const std::vector<TranscriptionSegment> &segments);

    // Chunked transcription for long audio (releases mutex between 30s chunks)
    void transcribeChunked(std::shared_ptr<TranscriptionJob> job,
                           std::vector<float> audio);

    httplib::Server m_server;
    std::thread m_serverThread;
    Transcriber *m_transcriber;
    int m_port = 0;
    std::atomic<bool> m_running{false};

    std::mutex m_jobsMutex;
    std::unordered_map<std::string, std::shared_ptr<TranscriptionJob>> m_jobs;
    int m_nextJobId = 1;
    std::atomic<bool> m_jobRunning{false}; // one-job-at-a-time guard
};
