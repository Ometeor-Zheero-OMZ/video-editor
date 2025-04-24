#include <media/media_file.h>
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_file>" << std::endl;
        return 1;
    }

    const char *input_filename = argv[1];

    video_codec::MediaFile media_file;
    if (!media_file.open(input_filename))
    {
        return 1;
    }

    media_file.printInfo();

    return 0;
}