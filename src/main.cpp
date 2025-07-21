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
#include <fstream>

#include "bmp.hpp"
#include "pixelbasher.hpp"

#include <vector>

void write_stats_to_csv(BMP auth, BMP import, BMP diff, int page_number, std::string basename,
     std::string filename);
// Main function to compare two BMP images and generate a diff image
// Usage: pixelbasher base.bmp input.bmp output.bmp [enable_minor_differences]
// The last argument is optional and can be "true" or "false" to enable
int main(int argc, char* argv[])
{
    // the format could be so all the auth pages for the file, then all the import pages for the file
    // Check if the correct number of arguments is provided
    if (argc < 6) {
        std::cout << "Incorrect usage: " << argv[0] << "filename.ext auth1.bmp auth2.bmp ... import1.bmp import2.bmp ... export1.bmp export2.bmp import_dir exported_dir <enable_lo_roundtrip> <enable_minor_differences>\n";
        return 1;
    }

    bool enable_minor_differences = false;
    bool enable_lo_roundtrip = false;
    int arg_index = argc;

    // Check if the last argument is a boolean flag for enabling minor differences
    if (std::string(argv[arg_index - 1]) == "true" || std::string(argv[arg_index - 1]) == "false") {
        enable_minor_differences = (std::string(argv[arg_index - 1]) == "true");
        arg_index--; // Decrease the argument count to exclude the flag
    }

    // Check if the second last argument is a boolean flag for enabling LO roundtrip
    if (std::string(argv[arg_index - 1]) == "true" || std::string(argv[arg_index - 1]) == "false") {
        enable_lo_roundtrip = (std::string(argv[arg_index - 1]) == "true");
        arg_index--; // Decrease the argument count to exclude the flag
    }

    // Output dir is the last argument
    if (arg_index < 6) {
        std::cerr << "Error: Not enough arguments provided.\n";
        return 1;
    }

    std::cout << "arg_index: " << arg_index << "\n";

    std::string import_dir = argv[arg_index - 2];
    std::string export_dir = argv[arg_index - 1];

    std::string basename = argv[1]; // The first argument is the basename of the file, not used in this context
    std::string extension = basename.substr(basename.find_last_of('.')).erase(0, 1); // Get the file extension without the dot

    std::cout << "extension: " << extension << "\n";

    int num_image_args = arg_index - 4; // from the count and the program name and basename(filename.doc)

    int pdf_count = enable_lo_roundtrip ? 3 : 2; // If LO roundtrip is enabled, we have 3 formats, otherwise 2

    // std::cout << "Number of image arguments: " << num_image_args << "\n";
    // std::cout << "PDF count: " << pdf_count << "\n";
    if (num_image_args % pdf_count != 0) {
        std::cerr << "Error: Mismatched number of authoritative and import pages.\n";
        return 1;
    }

    int num_pages = num_image_args / pdf_count; // Calculate the number of pages based on the number of arguments
    std::vector<BMP> authoritative_pages;
    std::vector<BMP> import_pages;
    std::vector<BMP> export_pages;

    for (int i = 0; i < num_pages; i++)
        authoritative_pages.push_back(BMP(argv[i + 2], basename)); // Plus one to skip the program name

    for (int i = 0; i < num_pages; i++)
        import_pages.push_back(BMP(argv[i + 2 + num_pages], basename)); // Import pages start after the authoritative pages

    if (enable_lo_roundtrip) {
        for (int i = 0; i < num_pages; i++)
            export_pages.push_back(BMP(argv[i + 2 + num_pages * 2], basename)); // Export pages start after the import pages
    }

    // std::cout << "Number of pages: " << num_pages << "\n";
    // std::cout << "Enable LO roundtrip: " << (enable_lo_roundtrip ? "true" : "false") << "\n";
    // std::cout << "Enable minor differences: " << (enable_minor_differences ? "true" : "false") << "\n";
    // std::cout << "Output directory: " << output_dir << "\n";

    for (int i = 0; i < num_pages; i++) {
        BMP base = authoritative_pages[i];
        BMP imported = import_pages[i];

        std::string import_diff_path = import_dir + extension + "/" + basename + "_import-page-" + std::to_string(i + 1) + ".bmp";

        PixelBasher pixel_basher;

        BMP diff_result = pixel_basher.compare_to_bmp(base, imported, enable_minor_differences);
        diff_result.write(import_diff_path.c_str());

        // std::cout << "Processed page " << (i + 1) << ": " << output_path << "\n";
        // std::cout << diff_result.print_stats() << "\n";

        if (enable_lo_roundtrip) {
            BMP& export_lo = export_pages[i];
            BMP export_diff_result = pixel_basher.compare_to_bmp(base, export_lo, enable_minor_differences);

            std::string export_output_path = export_dir + extension + "/" + basename + "_export-page-" + std::to_string(i + 1) + ".bmp";
            export_diff_result.write(export_output_path.c_str());

            // std::cout << "Processed export page " << (i + 1) << ": " << export_output_path << "\n";
            // std::cout << export_diff_result.print_stats() << "\n";
        }
        // Write stats to CSV
        std::string csv_filename = "diff-pdf-" + extension + "-import-statistics.csv";
        write_stats_to_csv(base, imported, diff_result, i + 1, basename, csv_filename);
    }

    return 0;
}

void write_stats_to_csv(BMP auth, BMP import, BMP diff, int page_number, std::string basename,
     std::string filename)
{
  std::ofstream csv_file(filename);
  if (!csv_file.is_open()) {
    throw std::runtime_error("Cannot open CSV file for writing");
  }

  double auth_total_pixels = auth.get_width() * auth.get_height();
  double import_total_pixels = import.get_width() * import.get_height();

  csv_file << basename << ","
            << page_number << ","
            << auth_total_pixels << "," // total pixels in auth
            << auth.get_non_background_count() << "," // non-background pixels in auth
            << (static_cast<double>(auth.get_non_background_count()) / auth_total_pixels) << "," // non-background percentage in auth
            << import_total_pixels << "," // total pixels in import
            << import.get_non_background_count() << "," // non-background pixels in import
            << (static_cast<double>(import.get_non_background_count()) / import_total_pixels)<< "," // non-background percentage in import
            << diff.get_red_count() << "," // red pixels in diff
            << (static_cast<double>(diff.get_red_count()) / import_total_pixels) << "\n";
}