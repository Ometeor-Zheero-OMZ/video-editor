#pragma once

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include <string>
#include <memory>
#include <vector>

namespace video_codec
{
    class StreamInfo
    {
    public:
        uint32_t index;
        AVMediaType type;
        std::string codec_name;
        std::string codec_long_name;

        // Video specific
        uint32_t width{0};
        uint32_t height{0};
        double frame_rate{0.0};

        // Audio specific
        uint32_t sample_rate{0};
        uint32_t channels{0};
    };

    class MediaFile
    {
    public:
        MediaFile() = default;
        ~MediaFile();

        // Display copy
        MediaFile(const MediaFile &) = delete;
        MediaFile &operator=(const MediaFile &) = delete;

        // Enable move
        MediaFile(MediaFile &&) noexcept;
        MediaFile &operator=(MediaFile &&) noexcept;

        bool open(const std::string &filename);
        void close();

        // Getters
        const std::string &getFilename() const { return filename_; }
        int64_t getDuration() const; // in seconds
        int64_t getBitRate() const;  // in bits per second
        const std::string &getFormatName() const { return format_name_; }
        const std::string &getFormatLongName() const { return format_long_name_; }

        // Stream information
        int32_t getNumStreams() const;
        const std::vector<StreamInfo> &getStreamInfo() const { return stream_info_; }

        // Print info
        void printInfo() const;

    private:
        std::string filename_;
        std::string format_name_;
        std::string format_long_name_;
        AVFormatContext *format_ctx_{nullptr};
        std::vector<StreamInfo> stream_info_;

        void analyzeStreams();
    };
}