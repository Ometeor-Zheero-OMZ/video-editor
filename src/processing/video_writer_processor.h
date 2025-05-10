#pragma once

#include <processing/frame_processor.h>
#include <media/video_writer.h>
#include <memory>

namespace video_codec
{
    class VideoWriterProcessor : public FrameProcessor
    {
    public:
        VideoWriterProcessor(const std::string &output_filename,
                             int width, int height, double fps = 30.0,
                             const std::string &codec = "libx264");

        virtual ~VideoWriterProcessor();

        bool processFrame(AVFrame *frame, int frame_number) override;

        // 動画出力を終了
        bool finalize();

        std::string getLastError() const;

    private:
        std::unique_ptr<VideoWriter> writer_;
        bool initialized_{false};
        bool finalized_{false};
    };
}