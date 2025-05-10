#include <processing/video_writer_processor.h>
#include <iostream>

namespace video_codec
{
    VideoWriterProcessor::VideoWriterProcessor(const std::string &output_filename,
                                               int width, int height, double fps,
                                               const std::string &codec)
        : writer_(std::make_unique<VideoWriter>())
    {
        if (writer_->open(output_filename, width, height, fps, codec))
        {
            initialized_ = true;
            std::cout << "VideoWriter opened successfully: " << output_filename << std::endl;
            std::cout << "  Resolution: " << width << "x" << height << std::endl;
            std::cout << "  FPS: " << fps << std::endl;
            std::cout << "  Codec: " << codec << std::endl;
        }
        else
            std::cerr << "Failed to open VideoWriter: " << writer_->getLastError() << std::endl;
    }

    VideoWriterProcessor::~VideoWriterProcessor()
    {
        if (initialized_ && !finalized_)
            finalize();
    }

    bool VideoWriterProcessor::processFrame(AVFrame *frame, int frame_number)
    {
        if (!initialized_)
        {
            std::cerr << "VideoWriterProcessor not initialized" << std::endl;
            return false;
        }

        if (finalized_)
        {
            std::cerr << "VideoWriterProcessor already finalized" << std::endl;
            return false;
        }

        std::cout << "Writing frame #" << frame_number << std::endl;

        if (!writer_->writeFrame(frame))
        {
            std::cerr << "Failed to write frame: " << writer_->getLastError() << std::endl;
            return false;
        }

        return true;
    }

    bool VideoWriterProcessor::finalize()
    {
        if (!initialized_)
            return true; // 初期化されていない場合は何もしない

        if (finalized_)
            return true; // すでに終了済み

        std::cout << "Finalizing video output..." << std::endl;

        bool result = writer_->close();
        finalized_ = true;

        if (result)
            std::cout << "Video output completed successfully" << std::endl;
        else
            std::cerr << "Failed to finalize video output: " << writer_->getLastError() << std::endl;

        return result;
    }

    std::string VideoWriterProcessor::getLastError() const
    {
        return writer_->getLastError();
    }
}