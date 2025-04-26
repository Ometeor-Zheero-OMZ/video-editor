#pragma once

#include <processing/frame_processor.h>
#include <string>
#include <iostream>
#include <filesystem>

extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/imgutils.h>
}

namespace video_codec
{
    // Simple processor that just displays frame information
    class SimpleFrameProcessor : public FrameProcessor
    {
    public:
        SimpleFrameProcessor() = default;

        bool processFrame(AVFrame *frame, int frame_number) override
        {
            std::cout << "Processing frame #" << frame_number
                      << " (size: " << frame->width << "x" << frame->height << ")" << std::endl;

            return true;
        }
    };

    // Frame saver processor - saves frames as image files
    class FrameSaverProcessor : public FrameProcessor
    {
    public:
        explicit FrameSaverProcessor(const std::string &output_dir, int save_interval = 1,
                                     const std::string &format = "jpg")
            : output_dir_(output_dir), save_interval_(save_interval), format_(format)
        {
            if (!std::filesystem::exists(output_dir_))
                std::filesystem::create_directories(output_dir_);
        }

        bool processFrame(AVFrame *frame, int frame_number) override;

    private:
        std::string output_dir_;
        int save_interval_;
        std::string format_;
    };

    // Base class for filter-based processors
    class FilterProcessor : public FrameProcessor
    {
    public:
        FilterProcessor(const std::string &filter_desc, FrameProcessor *next_processor = nullptr);
        virtual ~FilterProcessor();

        bool processFrame(AVFrame *frame, int frame_number) override;

        // Add method to set next processor
        void setNextProcessor(FrameProcessor *next_processor)
        {
            next_processor_ = next_processor;
        }

    protected:
        bool initFilterGraph(int width, int height, AVPixelFormat pix_fmt);
        void cleanup();

        FrameProcessor *next_processor_;
        std::string filter_desc_;

        AVFilterGraph *filter_graph_ = nullptr;
        AVFilterContext *buffersrc_ctx_ = nullptr;
        AVFilterContext *buffersink_ctx_ = nullptr;
        AVFrame *filtered_frame_ = nullptr;
        bool initialized_ = false;
    };

    // Grayscale processor using FFmpeg filters
    class GrayscaleProcessor : public FilterProcessor
    {
    public:
        explicit GrayscaleProcessor(FrameProcessor *next_processor = nullptr)
            : FilterProcessor("format=gray,format=rgb24", next_processor) {}
    };

    // Brightness/contrast processor using FFmpeg filters
    class BrightnessContrastProcessor : public FilterProcessor
    {
    public:
        explicit BrightnessContrastProcessor(double brightness, double contrast,
                                             FrameProcessor *next_processor = nullptr)
            : FilterProcessor(buildFilterString(brightness, contrast), next_processor) {}

    private:
        static std::string buildFilterString(double brightness, double contrast);
    };
}