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

#include <vector>
// Main function to compare two BMP images and generate a diff image
// Usage: pixelbasher base.bmp input.bmp output.bmp [enable_minor_differences]
// The last argument is optional and can be "true" or "false" to enable
int main(int argc, char* argv[])
{
    // the format could be so all the auth pages for the file, then all the import pages for the file
    // Check if the correct number of arguments is provided
    if (argc < 4) {
        std::cout << "Incorrect usage: " << argv[0] << " auth1.bmp auth2.bmp ... import1.bmp import2.bmp ... output_dir [enable_minor_differences]\n";
        return 1;
    }

    // Nee do determine whether optional flag is present
    bool enable_minor_differences = false;
    std::string last_arg = argv[argc - 1];
    bool has_flag = (last_arg == "true" || last_arg == "false");
    if (has_flag) {
        enable_minor_differences = (last_arg == "true");
    }

    int num_inputs = argc - (has_flag ? 3 : 2);
    if (num_inputs % 2 != 0) {
        std::cerr << "Error: Mismatched number of authoritative and import pages";
        return 1;
    }

    int num_pages = num_inputs / 2;
    std::vector<BMP> authortiative_pages;
    std::vector<BMP> import_pages;

    // Load authoritative BMP's
    for (int i = 1; i <= num_pages; i++) {
        const char* auth_path = argv[i];
        authortiative_pages.push_back(auth_path);
    }

    // Load import BMP's
    for (int i = num_pages + 1; i <= num_inputs; i++) {
        const char* import_path = argv[i];
        import_pages.push_back(import_path);
    }

    const char* output_dir = argv[num_inputs + 1];
    PixelBasher pixel_basher;
    std::string file_stats = "Processing " + std::string(output_dir) + "\n";
    for (int i = 0; i < num_pages; i++) {
        BMP& base = authortiative_pages[i];
        BMP& import = import_pages[i];
        std::string output_path = std::string(output_dir) + "diff-page-" + std::to_string(i + 1) + ".bmp";

        pixel_basher.compare_to_bmp(base, import, enable_minor_differences); // should change compare_to_bmp to a static method
        base.write(output_path.c_str());

        file_stats += base.print_stats() + "\n";
    }

    std::cout << file_stats << "\n";
    return 0;
}
