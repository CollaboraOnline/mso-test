#include <iostream>

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

    BMP base(authoritative_path);
    BMP input(import_path);

    pixelBasher.compareToBMP(base, input);
    base.write(output_path);
    return 0;

}
