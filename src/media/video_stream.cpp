#include <media/video_stream.h>
#include <processing/frame_processor.h>
#include <iostream>

namespace video_codec
{
    VideoStream::~VideoStream()
    {
        cleanup();
    }

    VideoStream::VideoStream(VideoStream &&other) noexcept
        : format_ctx_(other.format_ctx_),
          codec_ctx_(other.codec_ctx_),
          codec_(other.codec_),
          stream_index_(other.stream_index_),
          frame_(other.frame_),
          frame_rgb_(other.frame_rgb_),
          sws_ctx_(other.sws_ctx_),
          buffer_(other.buffer_)
    {
        other.format_ctx_ = nullptr;
        other.codec_ctx_ = nullptr;
        other.codec_ = nullptr;
        other.frame_ = nullptr;
        other.frame_rgb_ = nullptr;
        other.sws_ctx_ = nullptr;
        other.buffer_ = nullptr;
    }

    VideoStream &VideoStream::operator=(VideoStream &&other) noexcept
    {
        if (this != &other)
        {
            cleanup();

            format_ctx_ = other.format_ctx_;
            codec_ctx_ = other.codec_ctx_;
            codec_ = other.codec_;
            stream_index_ = other.stream_index_;
            frame_ = other.frame_;
            frame_rgb_ = other.frame_rgb_;
            sws_ctx_ = other.sws_ctx_;
            buffer_ = other.buffer_;

            other.format_ctx_ = nullptr;
            other.codec_ctx_ = nullptr;
            other.codec_ = nullptr;
            other.frame_ = nullptr;
            other.frame_rgb_ = nullptr;
            other.sws_ctx_ = nullptr;
            other.buffer_ = nullptr;
        }
        return *this;
    }

    bool VideoStream::initialize(AVFormatContext *format_ctx, int stream_index)
    {
        cleanup();

        if (!format_ctx || stream_index < 0 ||
            static_cast<unsigned int>(stream_index) >= format_ctx->nb_streams)
        {
            std::cerr << "Invalid format context or stream index" << std::endl;
            return false;
        }

        format_ctx_ = format_ctx;
        stream_index_ = stream_index;

        // Fetch codec params
        AVCodecParameters *codec_params = format_ctx_->streams[stream_index_]->codecpar;

        // Find codec
        codec_ = const_cast<AVCodec *>(avcodec_find_decoder(codec_params->codec_id));
        if (!codec_)
        {
            std::cerr << "Codec not found for stream" << stream_index_ << std::endl;
            return false;
        }

        // Allocate codec context
        codec_ctx_ = avcodec_alloc_context3(codec_);
        if (!codec_ctx_)
        {
            std::cerr << "Could not allocate codec context" << std::endl;
            return false;
        }

        // Copy codec params to the context
        if (avcodec_parameters_to_context(codec_ctx_, codec_params) < 0)
        {
            std::cerr << "Could not copy codec params to context" << std::endl;
            avcodec_free_context(&codec_ctx_);
            return false;
        }

        // Open codec
        if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0)
        {
            std::cerr << "Could not open codec" << std::endl;
            avcodec_free_context(&codec_ctx_);
            return false;
        }

        // Initialize frame buffer
        if (!initializeFrameBuffers())
        {
            avcodec_close(codec_ctx_);
            avcodec_free_context(&codec_ctx_);
            return false;
        }

        return true;
    }

    bool VideoStream::initializeFrameBuffers()
    {
        // Allocate frame
        frame_ = av_frame_alloc();
        if (!frame_)
        {
            std::cerr << "Could not allocate frame" << std::endl;
            return false;
        }

        // Allocate an RGB frame for conversion
        frame_rgb_ = av_frame_alloc();
        if (!frame_rgb_)
        {
            std::cerr << "Could not allocate RGB frame" << std::endl;
            av_frame_free(&frame_);
            return false;
        }

        // Set RGB frame properties
        frame_rgb_->format = AV_PIX_FMT_RGB24;
        frame_rgb_->width = codec_ctx_->width;
        frame_rgb_->height = codec_ctx_->height;

        // Allocate the buffer using av_image_alloc for proper alignment
        int ret = av_image_alloc(
            frame_rgb_->data, frame_rgb_->linesize,
            codec_ctx_->width, codec_ctx_->height,
            AV_PIX_FMT_RGB24, 32); // 32-byte alignment

        if (ret < 0)
        {
            std::cerr << "Could not allocate RGB buffer" << std::endl;
            av_frame_free(&frame_rgb_);
            av_frame_free(&frame_);
            return false;
        }

        buffer_ = frame_rgb_->data[0]; // Store the buffer pointer for cleanup

        // Initialize scaling context
        sws_ctx_ = sws_getContext(
            codec_ctx_->width, codec_ctx_->height, codec_ctx_->pix_fmt,
            codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx_)
        {
            std::cerr << "Could not initialize scaling context" << std::endl;
            av_freep(&frame_rgb_->data[0]);
            av_frame_free(&frame_rgb_);
            av_frame_free(&frame_);
            return false;
        }

        return true;
    }

    bool VideoStream::processFrames(FrameProcessor &processor, int max_frames)
    {
        if (!codec_ctx_ || !format_ctx_)
        {
            std::cerr << "VideoStream not properly initialized" << std::endl;
            return false;
        }

        AVPacket *packet = av_packet_alloc();
        if (!packet)
        {
            std::cerr << "Could not allocate packet" << std::endl;
            return false;
        }

        int frame_cnt = 0;
        bool result = true;

        // Seek to the beginning of the stream
        av_seek_frame(format_ctx_, stream_index_, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codec_ctx_);

        // Read packets
        while (av_read_frame(format_ctx_, packet) >= 0)
        {
            // Check if the packet belongs to the target video stream
            if (packet->stream_index == stream_index_)
            {
                // Decode packet
                int ret = avcodec_send_packet(codec_ctx_, packet);
                if (ret < 0)
                {
                    std::cerr << "Error sending packet for decoding" << std::endl;
                    result = false;
                    break;
                }

                while (ret >= 0)
                {
                    ret = avcodec_receive_frame(codec_ctx_, frame_);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    else if (ret < 0)
                    {
                        std::cerr << "Error during decoding" << std::endl;
                        result = false;
                        break;
                    }

                    /// Convert a frame with RGB
                    sws_scale(sws_ctx_, frame_->data, frame_->linesize, 0,
                              codec_ctx_->height, frame_rgb_->data, frame_rgb_->linesize);

                    // Set the frame dimensions
                    frame_rgb_->width = codec_ctx_->width;
                    frame_rgb_->height = codec_ctx_->height;
                    frame_rgb_->format = AV_PIX_FMT_RGB24;

                    // Process frame
                    if (!processor.processFrame(frame_rgb_, frame_cnt))
                    {
                        std::cerr << "Frame processing error" << std::endl;
                        result = false;
                        break;
                    }

                    frame_cnt++;

                    // Terminate when the maximum frame count is reached
                    if (max_frames > 0 && frame_cnt >= max_frames)
                        break;
                }
            }

            // Free packet
            av_packet_unref(packet);

            // Terminate when the maximum frame count is reached
            if (max_frames > 0 && frame_cnt >= max_frames)
                break;
        }

        // Flush the decoder to retrieve remaining frames
        avcodec_send_packet(codec_ctx_, nullptr);
        int ret = 0;
        while (ret >= 0)
        {
            ret = avcodec_receive_frame(codec_ctx_, frame_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0)
            {
                std::cerr << "Error during flushing" << std::endl;
                result = false;
                break;
            }

            // Convert a frame with RGB
            sws_scale(sws_ctx_, frame_->data, frame_->linesize, 0,
                      codec_ctx_->height, frame_rgb_->data, frame_rgb_->linesize);

            // Set the frame dimensions
            frame_rgb_->width = codec_ctx_->width;
            frame_rgb_->height = codec_ctx_->height;
            frame_rgb_->format = AV_PIX_FMT_RGB24;

            // Process frame
            if (!processor.processFrame(frame_rgb_, frame_cnt))
            {
                std::cerr << "Frame processing error during flushing" << std::endl;
                result = false;
                break;
            }

            frame_cnt++;

            // Terminate when the maximum frame count is reached
            if (max_frames > 0 && frame_cnt >= max_frames)
                break;
        }

        av_packet_free(&packet);
        std::cout << "Processed " << frame_cnt << " frames" << std::endl;
        return result;
    }

    double VideoStream::getFrameRate() const
    {
        if (!format_ctx_ || stream_index_ < 0)
        {
            return 0.0;
        }

        AVStream *stream = format_ctx_->streams[stream_index_];
        return av_q2d(stream->r_frame_rate);
    }

    void VideoStream::cleanup()
    {
        if (sws_ctx_)
        {
            sws_freeContext(sws_ctx_);
            sws_ctx_ = nullptr;
        }

        if (frame_rgb_)
        {
            av_freep(&frame_rgb_->data[0]);
            av_frame_free(&frame_rgb_);
        }

        if (frame_)
        {
            av_frame_free(&frame_);
        }

        if (codec_ctx_)
        {
            avcodec_close(codec_ctx_);
            avcodec_free_context(&codec_ctx_);
        }

        // Note: format_ctx_ is managed externally, so it should not be freed here.
        format_ctx_ = nullptr;
        stream_index_ = -1;
        buffer_ = nullptr;
    }
}