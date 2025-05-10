#include <media/video_writer.h>
#include <iostream>
#include <sstream>

namespace video_codec
{
    VideoWriter::VideoWriter()
    {
    }

    VideoWriter::~VideoWriter()
    {
        cleanup();
    }

    void VideoWriter::cleanup()
    {
        if (sws_ctx_)
        {
            sws_freeContext(sws_ctx_);
            sws_ctx_ = nullptr;
        }

        if (yuv_frame_)
            av_frame_free(&yuv_frame_);

        if (codec_ctx_)
        {
            avcodec_close(codec_ctx_);
            avcodec_free_context(&codec_ctx_);
        }

        if (format_ctx_)
        {
            if (!(format_ctx_->oformat->flags & AVFMT_NOFILE))
                avio_closep(&format_ctx_->pb);

            avformat_free_context(format_ctx_);
            format_ctx_ = nullptr;
        }

        frame_count_ = 0;
    }

    bool VideoWriter::open(const std::string &filename, int width, int height,
                           double fps, const std::string &codec)
    {
        cleanup();

        width_ = width;
        height_ = height;
        fps_ = fps;

        // 出力フォーマットコンテキストの作成
        int ret = avformat_alloc_output_context2(&format_ctx_, nullptr, nullptr, filename.c_str());
        if (ret < 0)
        {
            setError("Could not allocate output format context", ret);
            return false;
        }

        // エンコーダーの初期化
        if (!initializeEncoder(codec))
        {
            cleanup();
            return false;
        }

        // スケーラーの初期化
        if (!initializeScaler())
        {
            cleanup();
            return false;
        }

        // YUVフレームの初期化
        if (!initializeYUVFrame())
        {
            cleanup();
            return false;
        }

        // 出力ファイルを開く
        if (!(format_ctx_->oformat->flags & AVFMT_NOFILE))
        {
            ret = avio_open(&format_ctx_->pb, filename.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0)
            {
                setError("Could not open output file", ret);
                cleanup();
                return false;
            }
        }

        // ヘッダーを書き込む
        ret = avformat_write_header(format_ctx_, nullptr);
        if (ret < 0)
        {
            setError("Could not write header", ret);
            cleanup();
            return false;
        }

        return true;
    }

    bool VideoWriter::initializeEncoder(const std::string &codec_name)
    {
        // エンコーダーを見つける
        const AVCodec *codec = avcodec_find_encoder_by_name(codec_name.c_str());
        if (!codec)
        {
            setError("Codec not found: " + codec_name);
            return false;
        }

        // ビデオストリームを追加
        video_stream_ = avformat_new_stream(format_ctx_, nullptr);
        if (!video_stream_)
        {
            setError("Could not allocate video stream");
            return false;
        }

        video_stream_->id = format_ctx_->nb_streams - 1;

        // エンコーダーコンテキストを作成
        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_)
        {
            setError("Could not allocate encoding context");
            return false;
        }

        // エンコーダーパラメータを設定
        codec_ctx_->width = width_;
        codec_ctx_->height = height_;
        codec_ctx_->time_base = {1, static_cast<int>(fps_)};
        codec_ctx_->framerate = {static_cast<int>(fps_), 1};
        codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

        // H.264用の設定
        if (codec->id == AV_CODEC_ID_H264)
        {
            codec_ctx_->bit_rate = width_ * height_ * 4; // 適切なビットレート
            av_opt_set(codec_ctx_->priv_data, "preset", "ultrafast", 0);
            av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);
        }

        // グローバルヘッダーフラグの設定
        if (format_ctx_->oformat->flags & AVFMT_GLOBALHEADER)
            codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        // エンコーダーを開く
        int ret = avcodec_open2(codec_ctx_, codec, nullptr);
        if (ret < 0)
        {
            setError("Could not open codec", ret);
            return false;
        }

        // ストリームパラメータをコピー
        ret = avcodec_parameters_from_context(video_stream_->codecpar, codec_ctx_);
        if (ret < 0)
        {
            setError("Could not copy stream parameters", ret);
            return false;
        }

        return true;
    }

    bool VideoWriter::initializeScaler()
    {
        // RGB24 から YUV420P への変換コンテキストを作成
        sws_ctx_ = sws_getContext(
            width_, height_, AV_PIX_FMT_RGB24,
            width_, height_, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx_)
        {
            setError("Could not initialize sws context");
            return false;
        }

        return true;
    }

    bool VideoWriter::initializeYUVFrame()
    {
        yuv_frame_ = av_frame_alloc();
        if (!yuv_frame_)
        {
            setError("Could not allocate YUV frame");
            return false;
        }

        yuv_frame_->format = AV_PIX_FMT_YUV420P;
        yuv_frame_->width = width_;
        yuv_frame_->height = height_;

        // フレームバッファを割り当て
        int ret = av_frame_get_buffer(yuv_frame_, 32);
        if (ret < 0)
        {
            setError("Could not allocate YUV frame buffer", ret);
            return false;
        }

        return true;
    }

    bool VideoWriter::writeFrame(AVFrame *frame)
    {
        if (!format_ctx_ || !codec_ctx_ || !yuv_frame_)
        {
            setError("VideoWriter not properly initialized");
            return false;
        }

        // フレームが書き込み可能か確認
        int ret = av_frame_make_writable(yuv_frame_);
        if (ret < 0)
        {
            setError("Could not make YUV frame writable", ret);
            return false;
        }

        // RGB24 から YUV420P に変換
        ret = sws_scale(
            sws_ctx_,
            frame->data, frame->linesize, 0, height_,
            yuv_frame_->data, yuv_frame_->linesize);

        if (ret <= 0)
        {
            setError("Error during color space conversion", ret);
            return false;
        }

        // タイムスタンプを設定
        yuv_frame_->pts = frame_count_;

        // フレームをエンコーダーに送信
        ret = avcodec_send_frame(codec_ctx_, yuv_frame_);
        if (ret < 0)
        {
            setError("Error sending frame to encoder", ret);
            return false;
        }

        // エンコードされたパケットを取得して書き込む
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;

        while (ret >= 0)
        {
            ret = avcodec_receive_packet(codec_ctx_, &pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0)
            {
                setError("Error receiving packet from encoder", ret);
                av_packet_unref(&pkt);
                return false;
            }

            // タイムスタンプを調整
            av_packet_rescale_ts(&pkt, codec_ctx_->time_base, video_stream_->time_base);
            pkt.stream_index = video_stream_->index;

            // パケットをファイルに書き込む
            ret = av_interleaved_write_frame(format_ctx_, &pkt);
            if (ret < 0)
            {
                setError("Error writing packet", ret);
                av_packet_unref(&pkt);
                return false;
            }

            av_packet_unref(&pkt);
        }

        frame_count_++;
        return true;
    }

    bool VideoWriter::close()
    {
        if (!format_ctx_)
            return true; // 既に閉じている

        // 残りのフレームをフラッシュ
        int ret = avcodec_send_frame(codec_ctx_, nullptr);
        if (ret < 0)
        {
            setError("Error flushing encoder", ret);
            return false;
        }

        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;

        while (true)
        {
            ret = avcodec_receive_packet(codec_ctx_, &pkt);
            if (ret == AVERROR_EOF)
                break;
            else if (ret < 0)
            {
                setError("Error receiving packet during flush", ret);
                av_packet_unref(&pkt);
                return false;
            }

            av_packet_rescale_ts(&pkt, codec_ctx_->time_base, video_stream_->time_base);
            pkt.stream_index = video_stream_->index;

            ret = av_interleaved_write_frame(format_ctx_, &pkt);
            if (ret < 0)
            {
                setError("Error writing packet during flush", ret);
                av_packet_unref(&pkt);
                return false;
            }

            av_packet_unref(&pkt);
        }

        // トレーラーを書き込む
        ret = av_write_trailer(format_ctx_);
        if (ret < 0)
        {
            setError("Error writing trailer", ret);
            return false;
        }

        // リソースをクリーンアップ
        cleanup();

        return true;
    }

    void VideoWriter::setError(const std::string &message, int error_code)
    {
        std::ostringstream oss;
        oss << message;

        if (error_code != 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(error_code, errbuf, AV_ERROR_MAX_STRING_SIZE);
            oss << ": " << errbuf;
        }

        last_error_ = oss.str();
        std::cerr << last_error_ << std::endl;
    }
}