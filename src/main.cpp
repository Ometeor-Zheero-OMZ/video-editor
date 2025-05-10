#include <media/media_file.h>
#include <processing/frame_processor.h>
#include <processing/simple_frame_processor.h>
#include <processing/video_writer_processor.h>
#include <iostream>
#include <string>
#include <memory>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_file> [output_dir] [max_frames]" << std::endl;
        return 1;
    }

    const char *input_filename = argv[1];

    std::string output_dir = "./frames";
    if (argc > 2)
        output_dir = argv[2];

    int max_frames = -1;
    if (argc > 3)
        max_frames = std::stoi(argv[3]);

    video_codec::MediaFile media_file;
    if (!media_file.open(input_filename))
    {
        return 1;
    }

    media_file.printInfo();

    std::cout << "\nSelect frame processing option:" << std::endl;
    std::cout << "1. Display frame information only" << std::endl;
    std::cout << "2. Save frames as JPG images" << std::endl;
    std::cout << "3. Convert to grayscale and save frames" << std::endl;
    std::cout << "4. Adjust brightness/contrast and save frames" << std::endl;
    std::cout << "5. Create MP4 video output" << std::endl;
    std::cout << "Option: ";

    int option;
    std::cin >> option;

    bool result = false;

    switch (option)
    {
    case 1:
    {
        video_codec::SimpleFrameProcessor processor;
        result = media_file.processVideoFrames(processor, max_frames);
        break;
    }
    case 2:
    {
        int save_interval;
        std::cout << "Enter frame save interval (1 = every frame, 5 = every 5th frame, etc.): ";
        std::cin >> save_interval;

        std::string format;
        std::cout << "Enter image format (jpg, png, bmp): ";
        std::cin >> format;

        video_codec::FrameSaverProcessor processor(output_dir, save_interval, format);
        result = media_file.processVideoFrames(processor, max_frames);
        break;
    }
    case 3:
    {
        int save_interval;
        std::cout << "Enter frame save interval: ";
        std::cin >> save_interval;

        std::string format;
        std::cout << "Enter image format (jpg, png, bmp): ";
        std::cin >> format;

        auto saver = std::make_shared<video_codec::FrameSaverProcessor>(output_dir, save_interval, format);
        auto grayscale = std::make_shared<video_codec::GrayscaleProcessor>();
        grayscale->setNextProcessor(saver.get());

        result = media_file.processVideoFrames(*grayscale, max_frames);
        break;
    }
    case 4:
    {
        int save_interval;
        std::cout << "Enter frame save interval: ";
        std::cin >> save_interval;

        std::string format;
        std::cout << "Enter image format (jpg, png, bmp): ";
        std::cin >> format;

        double brightness, contrast;
        std::cout << "Enter brightness adjustment (-1.0 to 1.0): ";
        std::cin >> brightness;
        std::cout << "Enter contrast adjustment (0.0 to 3.0, 1.0 is normal): ";
        std::cin >> contrast;

        auto saver = std::make_shared<video_codec::FrameSaverProcessor>(output_dir, save_interval, format);
        video_codec::BrightnessContrastProcessor processor(brightness, contrast, saver.get());

        result = media_file.processVideoFrames(processor, max_frames);
        break;
    }
    case 5:
    {
        std::string raw_filename;
        std::cout << "Enter output video filename (e.g., output.mp4): ";
        std::cin >> raw_filename;

        std::string output_directory = "output_videos";
        if (!std::filesystem::exists(output_directory))
            std::filesystem::create_directories(output_directory);

        std::string output_filename = output_directory + "/" + raw_filename;

        double fps;
        std::cout << "Enter output FPS (default 30): ";
        std::cin >> fps;
        if (fps <= 0)
            fps = 30.0;

        // Get video stream information for resolution
        video_codec::VideoStream stream = media_file.getVideoStream();
        if (!stream.getCodecContext())
        {
            std::cerr << "Failed to get video strema information" << std::endl;
            return 1;
        }

        int width = stream.getWidth();
        int height = stream.getHeight();

        std::cout << "Creating video output: " << output_filename << std::endl;
        std::cout << "Resolution: " << width << "x" << height << std::endl;
        std::cout << "FPS: " << fps << std::endl;

        // Option for filters before saving
        std::cout << "\nApply filters?" << std::endl;
        std::cout << "1. No filters" << std::endl;
        std::cout << "2. Grayscale filter" << std::endl;
        std::cout << "3. Brightness/contrast adjustment" << std::endl;
        std::cout << "Option: ";

        int filter_option;
        std::cin >> filter_option;

        std::unique_ptr<video_codec::VideoWriterProcessor> video_writer;
        video_writer = std::make_unique<video_codec::VideoWriterProcessor>(
            output_filename, width, height, fps);

        switch (filter_option)
        {
        case 1:
            // No filters
            result = media_file.processVideoFrames(*video_writer, max_frames);
            break;

        case 2:
            // Grayscale
            {
                auto grayscale = std::make_unique<video_codec::GrayscaleProcessor>(video_writer.get());
                result = media_file.processVideoFrames(*grayscale, max_frames);
                break;
            }

        case 3:
            // Brightness/contrast adjustment
            {
                double brightness, contrast;
                std::cout << "Enter brightness adjustment (-1.0 to 1.0): ";
                std::cin >> brightness;
                std::cout << "Enter contrast adjustment (0.0 to 3.0, 1.0 is normal): ";
                std::cin >> contrast;

                auto brightness_contrast = std::make_unique<video_codec::BrightnessContrastProcessor>(
                    brightness, contrast, video_writer.get());
                result = media_file.processVideoFrames(*brightness_contrast, max_frames);
                break;
            }

        default:
            std::cerr << "Invalid option" << std::endl;
            return 1;
        }

        // Finalize video output
        if (result && !video_writer->finalize())
        {
            std::cerr << "Failed to finalize video output" << std::endl;
            result = false;
        }
        break;
    }
    default:
        std::cerr << "Invalid option" << std::endl;
        return 1;
    }

    if (!result)
    {
        std::cerr << "Frame processing failed" << std::endl;
        return 1;
    }

    std::cout << "Processing completed successfully" << std::endl;
    return 0;
}