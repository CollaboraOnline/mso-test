#include <iostream>
#include <cstdlib>

#include "bmp.hpp"
#include "pixelbasher.hpp"

int main(int argc, char* argv[])
{
    if (argc < 4) {
        std::cout << "Incorrect usage: " << argv[0] << " base.bmp input.bmp output.bmp\n";
        return 1;
    }

    PixelBasher pixelBasher;
    const char* authoritative_path = argv[1];
    const char* import_path = argv[2];
    const char* output_path = argv[3];
    bool enable_minor_differences = false;

    for (int i = 4; i < argc; i++) {
        std::string arg = argv[4];
        if (arg == "--enable-minor-differences") {
            enable_minor_differences = true;
        } else {
            std::cout << "Unknown argument. Do you mean --enable-minor-differences";
            return -1;
        }
    }

    BMP base(authoritative_path);
    BMP input(import_path);

    pixelBasher.compareToBMP(base, input, enable_minor_differences);
    base.write(output_path);
    return 0;

}
