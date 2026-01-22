"""Entry point for MedASR application."""

import logging
import sys


def setup_logging():
    """Configure logging."""
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        handlers=[
            logging.StreamHandler(sys.stdout),
            logging.FileHandler('medasr.log')
        ]
    )


def main():
    """Main entry point."""
    setup_logging()
    logger = logging.getLogger(__name__)

    try:
        from .app import MedASRApp
        app = MedASRApp()
        exit_code = app.run()
        app.cleanup()
        return exit_code
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
        return 0
    except Exception as e:
        logger.error(f"Fatal error: {e}", exc_info=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
