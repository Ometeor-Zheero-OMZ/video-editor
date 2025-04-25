#pragma once

extern "C"
{
#include <libavutil/frame.h>
}

namespace video_codec
{
    class FrameProcessor
    {
    public:
        virtual ~FrameProcessor() = default;

        // Processing frame
        // - frame: frame to be processed (RGB)
        // - frame_number: frame number to recognize the time
        virtual bool processFrame(AVFrame *frame, int frame_number) = 0;
    };
}