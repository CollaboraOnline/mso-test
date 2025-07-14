//
//
// Copyright the mso-test contributors
//
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>
#include <algorithm>
#include <cstdlib>

#include "bmp.hpp"
#include "pixelbasher.hpp"

// Main function to compare two BMP images and generate a diff image
// Usage: pixelbasher base.bmp input.bmp output.bmp [enable_minor_differences]
// The last argument is optional and can be "true" or "false" to enable
int main(int argc, char* argv[])
{
    // Check if the correct number of arguments is provided
    if (argc < 4) {
        std::cout << "Incorrect usage: " << argv[0] << " base.bmp input.bmp output.bmp\n";
        return -1;
    }

    PixelBasher pixel_basher;
    const char* authoritative_path = argv[1];
    const char* import_path = argv[2];
    const char* output_path = argv[3];
    bool enable_minor_differences = false;

    for (int i = 4; i < argc; i++) {
        std::string arg = argv[4];
        std::transform(arg.begin(), arg.end(), arg.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (arg == "false" || arg == "true") {
            enable_minor_differences = arg == "true" ? true : false;
        } else {
            std::cout << "Unknown argument. Do you mean true or false ? " << arg << std::endl;
            return -1;
        }
    }

    // Load the BMP images
    BMP base(authoritative_path);
    BMP input(import_path);

    // Compare the images and generate the diff onto the base (authoritative) image
    pixel_basher.compare_to_bmp(base, input, enable_minor_differences);
    base.print_stats();

    // Write the modified base image to the output path
    base.write(output_path);
    return 0;
}
