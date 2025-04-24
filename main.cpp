extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    // コマンドライン引数のチェック
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_file>" << std::endl;
        return 1;
    }

    const char *input_filename = argv[1];

    // フォーマットコンテキストの初期化
    AVFormatContext *format_ctx = NULL;

    // ファイルを開く
    int ret = avformat_open_input(&format_ctx, input_filename, NULL, NULL);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Could not open video file: " << errbuf << std::endl;
        return 1;
    }

    // ストリーム情報の取得
    ret = avformat_find_stream_info(format_ctx, NULL);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Could not find stream info: " << errbuf << std::endl;
        avformat_close_input(&format_ctx);
        return 1;
    }

    // ファイル情報の表示
    std::cout << "File Info:" << std::endl;
    std::cout << "File Name: " << input_filename << std::endl;
    std::cout << "Duration: " << format_ctx->iformat->name << " (" << format_ctx->iformat->long_name << ")" << std::endl;
    std::cout << "Total Duration: " << format_ctx->duration / AV_TIME_BASE << " seconds" << std::endl;
    std::cout << "Bit Rate: " << format_ctx->bit_rate / 1000 << " kbps" << std::endl;

    // ストリーム情報の表示
    std::cout << "\nStream Info:" << std::endl;
    for (unsigned int i{0}; i < format_ctx->nb_streams; ++i)
    {
        AVStream *stream = format_ctx->streams[i];
        AVCodecParameters *codec_params = stream->codecpar;
        const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);

        std::string media_type;
        // clang-format off
        switch (codec_params->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO: media_type = "Video"; break;
            case AVMEDIA_TYPE_AUDIO: media_type = "Audio"; break;
            case AVMEDIA_TYPE_SUBTITLE: media_type = "Subtitle"; break;
            default: media_type = "Unknown"; break;
        }
        // clang-format on

        std::cout << "Stream #" << i << " - " << media_type << ":" << std::endl;
        std::cout << "  Codec: " << (codec ? codec->name : "Unknown") << " (" << (codec ? codec->long_name : "Unknown") << ")" << std::endl;

        if (codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            std::cout << "  Resolution: " << codec_params->width << "x" << codec_params->height << std::endl;
            std::cout << "  Frame Rate: " << av_q2d(stream->r_frame_rate) << " fps" << std::endl;
        }
        else if (codec_params->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            std::cout << "  Sample Rate: " << codec_params->sample_rate << " Hz" << std::endl;
            std::cout << "  Channels: " << codec_params->ch_layout.nb_channels << std::endl;
        }
    }

    // Clean up
    avformat_close_input(&format_ctx);

    return 0;
}