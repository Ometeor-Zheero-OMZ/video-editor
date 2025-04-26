#include <processing/simple_frame_processor.h>
#include <filesystem>
#include <sstream>
#include <iomanip>

extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace fs = std::filesystem;

namespace video_codec
{
    bool FrameSaverProcessor::processFrame(AVFrame *frame, int frame_number)
    {
        // Only save frames at specified interval
        if (frame_number % save_interval_ != 0)
        {
            return true;
        }

        // Generate filename
        std::ostringstream filename;
        filename << output_dir_ << "/frame_" << std::setw(5) << std::setfill('0') << frame_number << "." << format_;

        // Debug frame information
        std::cout << "Processing frame: " << frame_number << std::endl;
        std::cout << "  Format: " << frame->format << std::endl;
        std::cout << "  Width: " << frame->width << std::endl;
        std::cout << "  Height: " << frame->height << std::endl;

        // Check if frame data is valid
        if (!frame->data[0])
        {
            std::cerr << "Invalid frame data" << std::endl;
            return false;
        }

        // Create RGB frame
        AVFrame *rgb_frame = av_frame_alloc();
        if (!rgb_frame)
        {
            std::cerr << "Could not allocate RGB frame" << std::endl;
            return false;
        }

        // Set RGB frame properties
        rgb_frame->format = AV_PIX_FMT_RGB24;
        rgb_frame->width = frame->width;
        rgb_frame->height = frame->height;

        // Allocate buffer for RGB frame
        int ret = av_frame_get_buffer(rgb_frame, 32); // 32-byte alignment
        if (ret < 0)
        {
            std::cerr << "Could not allocate RGB frame buffer" << std::endl;
            av_frame_free(&rgb_frame);
            return false;
        }

        // Make sure the frame data is writable
        ret = av_frame_make_writable(rgb_frame);
        if (ret < 0)
        {
            std::cerr << "Could not make RGB frame writable" << std::endl;
            av_frame_free(&rgb_frame);
            return false;
        }

        // Create swscale context
        struct SwsContext *sws_ctx = nullptr;

        // Get source pixel format from frame
        AVPixelFormat src_pix_fmt = (AVPixelFormat)frame->format;

        // If source format is not valid, try to guess based on common formats
        if (src_pix_fmt == AV_PIX_FMT_NONE)
        {
            std::cerr << "Unknown pixel format, trying YUV420P..." << std::endl;
            src_pix_fmt = AV_PIX_FMT_YUV420P;
        }

        sws_ctx = sws_getContext(
            frame->width, frame->height, src_pix_fmt,
            rgb_frame->width, rgb_frame->height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx)
        {
            std::cerr << "Could not initialize swscale context" << std::endl;
            av_frame_free(&rgb_frame);
            return false;
        }

        // Convert frame to RGB
        ret = sws_scale(
            sws_ctx,
            (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height,
            rgb_frame->data, rgb_frame->linesize);

        if (ret <= 0)
        {
            std::cerr << "Error scaling frame: " << ret << std::endl;
            sws_freeContext(sws_ctx);
            av_frame_free(&rgb_frame);
            return false;
        }

        // Save as PNG (simpler approach using libpng or stb_image)
        // For this example, we'll write raw RGB data as a PPM file (simple format)
        FILE *f = fopen(filename.str().c_str(), "wb");
        if (!f)
        {
            std::cerr << "Could not open output file: " << filename.str() << std::endl;
            sws_freeContext(sws_ctx);
            av_frame_free(&rgb_frame);
            return false;
        }

        // If format is png or jpg, use a more sophisticated approach
        if (format_ == "png" || format_ == "jpg" || format_ == "jpeg")
        {
            // Use FFmpeg encoding
            AVCodecID codec_id = (format_ == "png") ? AV_CODEC_ID_PNG : AV_CODEC_ID_MJPEG;
            const AVCodec *codec = avcodec_find_encoder(codec_id);

            if (!codec)
            {
                std::cerr << "Codec not found" << std::endl;
                fclose(f);
                sws_freeContext(sws_ctx);
                av_frame_free(&rgb_frame);
                return false;
            }

            AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
            if (!codec_ctx)
            {
                std::cerr << "Could not allocate codec context" << std::endl;
                fclose(f);
                sws_freeContext(sws_ctx);
                av_frame_free(&rgb_frame);
                return false;
            }

            codec_ctx->width = rgb_frame->width;
            codec_ctx->height = rgb_frame->height;
            codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;
            codec_ctx->time_base = {1, 25};
            codec_ctx->compression_level = 5; // Medium compression

            ret = avcodec_open2(codec_ctx, codec, nullptr);
            if (ret < 0)
            {
                std::cerr << "Could not open codec" << std::endl;
                avcodec_free_context(&codec_ctx);
                fclose(f);
                sws_freeContext(sws_ctx);
                av_frame_free(&rgb_frame);
                return false;
            }

            AVPacket *pkt = av_packet_alloc();
            if (!pkt)
            {
                std::cerr << "Could not allocate packet" << std::endl;
                avcodec_free_context(&codec_ctx);
                fclose(f);
                sws_freeContext(sws_ctx);
                av_frame_free(&rgb_frame);
                return false;
            }

            ret = avcodec_send_frame(codec_ctx, rgb_frame);
            if (ret < 0)
            {
                std::cerr << "Error sending frame to encoder" << std::endl;
                av_packet_free(&pkt);
                avcodec_free_context(&codec_ctx);
                fclose(f);
                sws_freeContext(sws_ctx);
                av_frame_free(&rgb_frame);
                return false;
            }

            ret = avcodec_receive_packet(codec_ctx, pkt);
            if (ret < 0)
            {
                std::cerr << "Error receiving packet from encoder" << std::endl;
                av_packet_free(&pkt);
                avcodec_free_context(&codec_ctx);
                fclose(f);
                sws_freeContext(sws_ctx);
                av_frame_free(&rgb_frame);
                return false;
            }

            // Write encoded data to file
            fwrite(pkt->data, 1, pkt->size, f);

            // Clean up encoding resources
            av_packet_free(&pkt);
            avcodec_free_context(&codec_ctx);
        }
        else
        {
            // Fallback to PPM format (raw RGB)
            fprintf(f, "P6\n%d %d\n255\n", rgb_frame->width, rgb_frame->height);

            // Write RGB data
            for (int y = 0; y < rgb_frame->height; y++)
            {
                fwrite(rgb_frame->data[0] + y * rgb_frame->linesize[0], 1, rgb_frame->width * 3, f);
            }
        }

        fclose(f);

        // Clean up
        sws_freeContext(sws_ctx);
        av_frame_free(&rgb_frame);

        std::cout << "Saved frame #" << frame_number << " to " << filename.str() << std::endl;

        return true;
    }

    FilterProcessor::FilterProcessor(const std::string &filter_desc, FrameProcessor *next_processor)
        : filter_desc_(filter_desc), next_processor_(next_processor)
    {
        filtered_frame_ = av_frame_alloc();
    }

    FilterProcessor::~FilterProcessor()
    {
        cleanup();
        if (filtered_frame_)
            av_frame_free(&filtered_frame_);
    }

    bool FilterProcessor::initFilterGraph(int width, int height, AVPixelFormat pix_fmt)
    {
        char args[512];
        int ret;

        // Clean up any existing filter graph
        cleanup();

        // Create filter graph
        filter_graph_ = avfilter_graph_alloc();
        if (!filter_graph_)
        {
            std::cerr << "Failed to allocate filter graph" << std::endl;
            return false;
        }

        // Create buffer source filter (input)
        const AVFilter *buffersrc = avfilter_get_by_name("buffer");
        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=1/1:pixel_aspect=1/1",
                 width, height, pix_fmt);

        ret = avfilter_graph_create_filter(&buffersrc_ctx_, buffersrc, "in",
                                           args, nullptr, filter_graph_);
        if (ret < 0)
        {
            std::cerr << "Cannot create buffer source" << std::endl;
            return false;
        }

        // Create buffer sink filter (output)
        const AVFilter *buffersink = avfilter_get_by_name("buffersink");
        ret = avfilter_graph_create_filter(&buffersink_ctx_, buffersink, "out",
                                           nullptr, nullptr, filter_graph_);
        if (ret < 0)
        {
            std::cerr << "Cannot create buffer sink" << std::endl;
            return false;
        }

        // Set output pixel format
        enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE};
        ret = av_opt_set_int_list(buffersink_ctx_, "pix_fmts", pix_fmts,
                                  AV_PIX_FMT_NONE, 4);
        if (ret < 0)
        {
            std::cerr << "Cannot set output pixel format" << std::endl;
            return false;
        }

        // Create the filter from the description
        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs = avfilter_inout_alloc();

        outputs->name = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx_;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        inputs->name = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx_;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        ret = avfilter_graph_parse_ptr(filter_graph_, filter_desc_.c_str(),
                                       &inputs, &outputs, nullptr);
        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        if (ret < 0)
        {
            std::cerr << "Failed to parse filter description: " << filter_desc_ << std::endl;
            return false;
        }

        // Configure the filter graph
        ret = avfilter_graph_config(filter_graph_, nullptr);
        if (ret < 0)
        {
            std::cerr << "Failed to configure filter graph" << std::endl;
            return false;
        }

        initialized_ = true;
        return true;
    }

    bool FilterProcessor::processFrame(AVFrame *frame, int frame_number)
    {
        int ret;

        // Initialize filter graph if needed
        if (!initialized_)
        {
            if (!initFilterGraph(frame->width, frame->height,
                                 static_cast<AVPixelFormat>(frame->format)))
            {
                return false;
            }
        }

        // Push the frame into the filter graph
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx_, frame,
                                           AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
        {
            std::cerr << "Error while feeding the filter graph" << std::endl;
            return false;
        }

        // Pull filtered frames from the filter graph
        ret = av_buffersink_get_frame(buffersink_ctx_, filtered_frame_);
        if (ret < 0)
        {
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            {
                std::cerr << "Error while retrieving filtered frame" << std::endl;
                return false;
            }
            return true; // No output frame ready yet
        }

        // Pass the filtered frame to the next processor if any
        bool result = true;
        if (next_processor_)
        {
            result = next_processor_->processFrame(filtered_frame_, frame_number);
        }

        // Unreference the filtered frame for reuse
        av_frame_unref(filtered_frame_);

        return result;
    }

    void FilterProcessor::cleanup()
    {
        if (filter_graph_)
        {
            avfilter_graph_free(&filter_graph_);
            filter_graph_ = nullptr;
            buffersrc_ctx_ = nullptr;
            buffersink_ctx_ = nullptr;
            initialized_ = false;
        }
    }

    std::string BrightnessContrastProcessor::buildFilterString(double brightness, double contrast)
    {
        std::ostringstream filter_str;

        // FFmpeg eq filter: brightness is -1.0 to 1.0, but FFmpeg uses 0.0-2.0
        // contrast is 0.0-3.0 but FFmpeg might use -1000.0 to 1000.0
        double ffmpeg_brightness = brightness + 1.0;
        double ffmpeg_contrast = contrast * 100.0;

        filter_str << "eq=brightness=" << ffmpeg_brightness
                   << ":contrast=" << ffmpeg_contrast;

        return filter_str.str();
    }
}