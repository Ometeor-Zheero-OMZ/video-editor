#pragma once

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <string>
#include <memory>
#include <functional>

namespace video_codec
{
    class FrameProcessor;

    class VideoStream
    {
    public:
        VideoStream() = default;
        ~VideoStream();

        // Not Allowed to copy
        VideoStream(const VideoStream &) = delete;
        VideoStream &operator=(const VideoStream &) = delete;

        // Can move
        VideoStream(VideoStream &&) noexcept;
        VideoStream &operator=(VideoStream &&) noexcept;

        // Initialize Stream
        bool initialize(AVFormatContext *format_ctx, int stream_index);

        // Processing frames
        bool processFrames(FrameProcessor &processor, int max_frames = -1);

        // Getter
        int getWidth() const { return codec_ctx_ ? codec_ctx_->width : 0; }
        int getHeight() const { return codec_ctx_ ? codec_ctx_->height : 0; }
        AVPixelFormat getPixelFormat() const { return codec_ctx_ ? codec_ctx_->pix_fmt : AV_PIX_FMT_NONE; }
        double getFrameRate() const;
        AVCodecContext *getCodecContext() const { return codec_ctx_; }

    private:
        AVFormatContext *format_ctx_{nullptr};
        AVCodecContext *codec_ctx_{nullptr};
        AVCodec *codec_{nullptr};
        int stream_index_{-1};

        // Resource for processing frames
        AVFrame *frame_{nullptr};
        AVFrame *frame_rgb_{nullptr};
        SwsContext *sws_ctx_{nullptr};
        uint8_t *buffer_{nullptr};

        // Initialize Resource
        bool initializeFrameBuffers();

        // Free Resource
        void cleanup();
    };
}