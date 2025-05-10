#pragma once

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <string>
#include <memory>

namespace video_codec
{
    class VideoWriter
    {
    public:
        VideoWriter();
        ~VideoWriter();

        // 動画ファイルとして出力開始
        bool open(const std::string &filename, int width, int height,
                  double fps = 30.0, const std::string &codec = "libx264");

        // フレームを書き込む
        bool writeFrame(AVFrame *frame);

        // 動画ファイルを閉じて出力完了
        bool close();

        // エラーメッセージ取得
        const std::string &getLastError() const { return last_error_; }

    private:
        AVFormatContext *format_ctx_{nullptr};
        AVStream *video_stream_{nullptr};
        AVCodecContext *codec_ctx_{nullptr};
        // スケーリングコンテキスト（RGB->YUV変換用）
        SwsContext *sws_ctx_{nullptr};
        AVFrame *yuv_frame_{nullptr};

        int width_{0};
        int height_{0};
        double fps_{30.0};

        int64_t frame_count_{0};

        std::string last_error_;

        bool initializeEncoder(const std::string &codec_name);
        bool initializeScaler();
        bool initializeYUVFrame();

        void cleanup();

        void setError(const std::string &message, int error_code = 0);
    };
}