#include <media/media_file.h>
#include <iostream>

namespace video_codec
{
    MediaFile::~MediaFile()
    {
        close();
    }

    MediaFile::MediaFile(MediaFile &&other) noexcept
        : filename_(std::move(other.filename_)),
          format_name_(std::move(other.format_name_)),
          format_long_name_(std::move(other.format_long_name_)),
          format_ctx_(other.format_ctx_),
          stream_info_(std::move(other.stream_info_))
    {
        other.format_ctx_ = nullptr;
    }

    MediaFile &MediaFile::operator=(MediaFile &&other) noexcept
    {
        if (this != &other)
        {
            close();

            filename_ = (std::move(other.filename_));
            format_name_ = (std::move(other.format_name_));
            format_long_name_ = (std::move(other.format_long_name_));
            format_ctx_ = (other.format_ctx_);
            stream_info_ = (std::move(other.stream_info_));

            other.format_ctx_ = nullptr;
        }
        return *this;
    }

    bool MediaFile::open(const std::string &filename)
    {
        close();

        filename_ = filename;

        // Initialize format context
        int ret = avformat_open_input(&format_ctx_, filename.c_str(), NULL, NULL);
        if (ret < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Could not open video file: " << errbuf << std::endl;
            return false;
        }

        // Retrieve stream info
        ret = avformat_find_stream_info(format_ctx_, NULL);
        if (ret < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Could not find stream info: " << errbuf << std::endl;
            avformat_close_input(&format_ctx_);
            format_ctx_ = nullptr;
            return false;
        }

        // Save format info
        format_name_ = format_ctx_->iformat->name;
        format_long_name_ = format_ctx_->iformat->long_name;

        // Analyze streams
        analyzeStreams();

        return true;
    }

    void MediaFile::close()
    {
        if (format_ctx_)
        {
            avformat_close_input(&format_ctx_);
            format_ctx_ = nullptr;
        }
        stream_info_.clear();
    }

    // clang-format off
    int64_t MediaFile::getDuration() const
    {
        if (!format_ctx_) return 0;
        return format_ctx_->duration / AV_TIME_BASE; // Convert to seconds
    }
    // clang-format on

    // clang-format off
    int64_t MediaFile::getBitRate() const
    {
        if (!format_ctx_) return 0;
        return format_ctx_->bit_rate; // in bits per second
    }
    // clang-format on

    // clang-format off
    int32_t MediaFile::getNumStreams() const
    {
        if (!format_ctx_) return 0;
        return format_ctx_->nb_streams;
    }
    // clang-format on

    // clang-format off
    void MediaFile::analyzeStreams()
    {
        stream_info_.clear();

        if (!format_ctx_) return;

        for (unsigned int i {0}; i < format_ctx_->nb_streams; ++i)
        {
            AVStream *stream = format_ctx_->streams[i];
            AVCodecParameters *codec_params = stream->codecpar;
            const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);

            StreamInfo info;
            info.index = i;
            info.type = codec_params->codec_type;
            info.codec_name = codec ? codec->name : "Unknown";
            info.codec_long_name = codec ? codec->long_name : "Unknown";

            if (codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                info.width = codec_params->width;
                info.height = codec_params->height;
                info.frame_rate = av_q2d(stream->r_frame_rate);
            }
            else if (codec_params->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                info.sample_rate = codec_params->sample_rate;
                info.channels = codec_params->ch_layout.nb_channels;
            }

            stream_info_.push_back(info);
        }
    }
    // clang-format on

    // clang-format off
    void MediaFile::printInfo() const
    {
        std::cout << "File Info:" << std::endl;
        std::cout << "File Name: " << filename_ << std::endl;
        std::cout << "Format: " << format_name_ << " (" << format_long_name_ << ")" << std::endl;
        std::cout << "Total Duration: " << getDuration() << " seconds" << std::endl;
        std::cout << "Bit Rate: " << getBitRate() / 1000 << " kbps" << std::endl;

        std::cout << "\nStream Info:" << std::endl;
        for (const auto& info : stream_info_)
        {
            std::string media_type;
            switch (info.type)
            {
                case AVMEDIA_TYPE_VIDEO: media_type = "Video"; break;
                case AVMEDIA_TYPE_AUDIO: media_type = "Audio"; break;
                case AVMEDIA_TYPE_SUBTITLE: media_type = "Subtitle"; break;
                default: media_type = "Unknown"; break;
            }

            std::cout << "Stream #" << info.index << " - " << media_type << ":" << std::endl;
            std::cout << "  Codec: " << info.codec_name << " (" << info.codec_long_name << ")" << std::endl;
            
            if (info.type == AVMEDIA_TYPE_VIDEO) {
                std::cout << "  Resolution: " << info.width << "x" << info.height << std::endl;
                std::cout << "  Frame Rate: " << info.frame_rate << " fps" << std::endl;
            }
            else if (info.type == AVMEDIA_TYPE_AUDIO) {
                std::cout << "  Sample Rate: " << info.sample_rate << " Hz" << std::endl;
                std::cout << "  Channels: " << info.channels << std::endl;
            }
        }
    }
    // clang-format on
}