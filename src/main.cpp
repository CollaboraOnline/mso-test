//
//
// Copyright the mso-test contributors
//
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include "bmp.hpp"
#include "pixelbasher.hpp"

struct ParsedArguments
{
    std::string basename;
    std::string extension;
    std::string import_dir;
    std::string export_dir;
    std::string import_compare_dir;
    std::string export_compare_dir;
    std::string image_dump_dir;
    std::string stamp_dir;
    std::vector<BMP> ms_orig_images;
    std::vector<BMP> lo_images;
    std::vector<BMP> ms_conv_images;
    std::vector<BMP> lo_previous_images;
    std::vector<BMP> ms_conv_previous_images;
    bool enable_minor_differences;
    bool no_save_overlay;
    bool image_dump;
    bool lo_previous;
    bool ms_previous;
};

void write_stats_to_csv(const BMP &base, const BMP &current, const BMP &previous, const BMP &diff, int page_number, std::string basename,
                        std::string filename, bool previous_exists)
{
    std::ofstream csv_file(filename);
    if (!csv_file.is_open())
    {
        throw std::runtime_error("Cannot open CSV file for writing");
    }

    double base_total_pixels = base.get_width() * base.get_height();
    double current_total_pixels = current.get_width() * current.get_height();
    double base_non_background_count = base.get_non_background_pixel_count();
    double current_non_background_count = current.get_non_background_pixel_count();

    csv_file << basename << ","
             << page_number << ","
             << base_total_pixels << ","
             << base_non_background_count << ","
             << (static_cast<double>(base_non_background_count) / base_total_pixels) << ","
             << current_total_pixels << ","
             << current_non_background_count << ","
             << (static_cast<double>(current_non_background_count) / current_total_pixels) << ","
             << diff.get_red_count() << ","
             << (static_cast<double>(diff.get_red_count()) / current_total_pixels);

    if (previous_exists)
    {
        double previous_total_pixels = previous.get_width() * previous.get_height();
        double previous_non_background_count = previous.get_non_background_pixel_count();
        csv_file << "," << previous_total_pixels << ","
                 << previous_non_background_count << ","
                 << (static_cast<double>(previous_non_background_count) / previous_total_pixels) << ","
                 << previous.get_red_count() << ","
                 << (static_cast<double>(previous.get_red_count()) / previous_total_pixels);
    }
    csv_file << "\n";
}

void parse_flag(char *argv[], int &arg_index, bool &option, std::string option_name)
{
    std::string value = argv[arg_index - 1];
    if (value == "true" || value == "false")
    {
        option = (value == "true");
        arg_index--;
    }
    else
    {
        throw std::runtime_error("Incorrect usage for " + option_name + ": " + value + " should be true of false");
    }
}

void parse_flags(int &arg_index, char *argv[], ParsedArguments &args)
{
    assert(arg_index >= 5);
    parse_flag(argv, arg_index, args.enable_minor_differences, "minor-differences");
    parse_flag(argv, arg_index, args.no_save_overlay, "no_save_overlay");
    parse_flag(argv, arg_index, args.image_dump, "image_dump");
    parse_flag(argv, arg_index, args.ms_previous, "ms_previous");
    parse_flag(argv, arg_index, args.lo_previous, "lo_previous");
}

void parse_directories(int &arg_index, char *argv[], ParsedArguments &args)
{
    assert(arg_index >= 6);
    args.stamp_dir = argv[--arg_index];
    args.image_dump_dir = argv[--arg_index];
    args.export_compare_dir = argv[--arg_index];
    args.import_compare_dir = argv[--arg_index];
    args.export_dir = argv[--arg_index];
    args.import_dir = argv[--arg_index];
}

void parse_image_group(char *argv[], int start, int pages, std::vector<BMP> &images)
{
    for (int i = 0; i < pages; i++)
    {
        images.push_back(BMP(argv[start + i]));
    }
}

ParsedArguments parse_arguments(int argc, char *argv[], int pdf_count = 3)
{
    if (argc < 11)
    {
        throw std::runtime_error("Incorrect usage: " + std::string(argv[0]) + " filename.ext" +
                                 "ms_orig-1.bmp ms_orig-2.bmp ... lo-1.bmp lo-2.bmp ... ms_conv.bmp-1.bmp ms_conv-2.bmp ..." +
                                 "[lo_previous-1.bmp lo_previous-2.bmp ... ms_conv_previous-1.bmp ms_conv_previous-2.bmp ...]" +
                                 "import_dir/ exported_dir/ import-compare_dir/ export-compare_dir/ image-dump_dir/ stamp_dir/" +
                                 "[lo_previous] [ms_preivous] [image_dump] [no_save_overlay] [enable_minor_differences]");
    }

    ParsedArguments args;
    int arg_index = argc;

    parse_flags(arg_index, argv, args);
    parse_directories(arg_index, argv, args);

    args.basename = argv[1];
    size_t dot_pos = args.basename.find_last_of('.');
    args.extension = (dot_pos != std::string::npos) ? args.basename.substr(dot_pos + 1) : "";

    if (args.lo_previous)
        pdf_count++;
    if (args.ms_previous)
        pdf_count++;

    int num_image_args = arg_index - 2; // exclude program, basename
    if (num_image_args % pdf_count != 0)
    {
        throw std::runtime_error("Error: " + std::to_string(pdf_count) + " files cannot have a total of " + std::to_string(num_image_args) + " pages.");
    }

    int num_pages = num_image_args / pdf_count;
    int offset = 2;

    parse_image_group(argv, offset, num_pages, args.ms_orig_images);
    offset += num_pages;

    parse_image_group(argv, offset, num_pages, args.lo_images);
    offset += num_pages;

    parse_image_group(argv, offset, num_pages, args.ms_conv_images);
    offset += num_pages;

    if (args.lo_previous)
    {
        parse_image_group(argv, offset, num_pages, args.lo_previous_images);
        offset += num_pages;
    }

    if (args.ms_previous)
    {
        parse_image_group(argv, offset, num_pages, args.ms_conv_previous_images);
        offset += num_pages;
    }
    return args;
}

BMP diff(PixelBasher &pixel_basher, BMP &base, BMP &target, bool allow_minor_diffs)
{
    BMP diff = pixel_basher.compare_bmps(base, target, allow_minor_diffs);
    return diff;
}

int main(int argc, char *argv[])
{
    try
    {
        ParsedArguments args = parse_arguments(argc, argv);
        PixelBasher pixel_basher;
        const std::string csv_filename = "diff-pdf-" + args.extension;

        size_t num_pages = args.ms_orig_images.size();
        if (num_pages != args.lo_images.size() || num_pages != args.ms_conv_images.size())
        {
            throw std::runtime_error("Error: mismatched number of pages (" + std::to_string(num_pages) + ") between MS_ORIG, LO, MS_CONV and/or LO_PREVIOUS, MS_CONV_PREVIOUS");
        }

        for (size_t i = 0; i < num_pages; i++)
        {
            bool force_save_import = false;
            bool force_save_export = false;

            BMP base = args.ms_orig_images[i];
            BMP lo = args.lo_images[i];
            BMP ms_conv = args.ms_conv_images[i];
            BMP lo_previous;
            BMP ms_conv_previous;

            BMP lo_diff = diff(pixel_basher, base, lo, args.enable_minor_differences);
            BMP ms_conv_diff = diff(pixel_basher, base, ms_conv, args.enable_minor_differences);

            BMP lo_previous_diff;
            BMP ms_conv_previous_diff;
            BMP lo_compare;
            BMP ms_conv_compare;

            std::string page_ext = std::to_string(i + 1) + ".bmp";

            if (args.lo_previous)
            {
                lo_previous = args.lo_previous_images[i];
                lo_previous_diff = diff(pixel_basher, base, lo_previous, args.enable_minor_differences);

                lo_compare = pixel_basher.compare_regressions(base, lo_diff, lo_previous_diff);
                if (args.no_save_overlay)
                {
                    if (lo_diff.get_red_count() > lo_previous_diff.get_red_count())
                    {
                        force_save_import = true;
                    }
                }
            }

            if (args.ms_previous)
            {
                ms_conv_previous = args.ms_conv_previous_images[i];
                ms_conv_previous_diff = diff(pixel_basher, base, ms_conv_previous, args.enable_minor_differences);

                ms_conv_compare = pixel_basher.compare_regressions(base, ms_conv_diff, ms_conv_previous_diff);
                if (args.no_save_overlay)
                {
                    if (ms_conv_diff.get_red_count() > ms_conv_previous_diff.get_red_count())
                    {
                        force_save_export = true;
                    }
                }
            }

            if (!args.no_save_overlay || force_save_import)
            {
                std::string output_path = args.import_dir + "/" + args.basename + "_import-" + page_ext;
                lo_diff.write(output_path);

                if (args.lo_previous)
                {
                    output_path = args.import_dir + "/" + args.basename + "_prev-import-" + page_ext;
                    lo_previous_diff.write(output_path);

                    output_path = args.import_compare_dir + "/" + args.basename + "_import-compare-" + page_ext;
                    lo_compare.write(output_path);

                    if (args.image_dump)
                    {
                        output_path = args.image_dump_dir + "/" + args.basename + "_import-compare-" + page_ext;
                        lo_compare.write(output_path);
                    }
                }
            }

            if (!args.no_save_overlay || force_save_export)
            {
                std::string output_path = args.export_dir + "/" + args.basename + "_export-" + page_ext;
                ms_conv_diff.write(output_path);

                if (args.ms_previous)
                {
                    output_path = args.export_dir + "/" + args.basename + "_prev-export-" + page_ext;
                    ms_conv_previous_diff.write(output_path);

                    output_path = args.export_compare_dir + "/" + args.basename + "_export-compare-" + page_ext;
                    ms_conv_compare.write(output_path);

                    if (args.image_dump)
                    {
                        std::string output_path = args.image_dump_dir + "/" + args.basename + "_export-compare-" + page_ext;
                        ms_conv_compare.write(output_path);
                    }
                }
            }

            if (args.image_dump)
            {
                std::string output_path = args.image_dump_dir + "/" + args.basename + "_authoritative_original-" + page_ext;
                base.write(output_path);

                output_path = args.image_dump_dir + "/" + args.basename + "_import-grayscale-" + page_ext;
                lo.write(output_path);

                output_path = args.image_dump_dir + "/" + args.basename + "_export-grayscale-" + page_ext;
                ms_conv.write(output_path);

                output_path = args.image_dump_dir + "/" + args.basename + "_import-overlay-" + page_ext;
                lo_diff.write(output_path);

                output_path = args.image_dump_dir + "/" + args.basename + "_export-overlay-" + page_ext;
                ms_conv_diff.write(output_path);

                output_path = args.image_dump_dir + "/" + args.basename + "_import-side-by-side-" + page_ext;
                BMP::write_side_by_side(lo_diff, base, lo, args.stamp_dir, output_path);

                output_path = args.image_dump_dir + "/" + args.basename + "_export-side-by-side-" + page_ext;
                BMP::write_side_by_side(ms_conv_diff, base, ms_conv, args.stamp_dir, output_path);

                if (args.lo_previous)
                {
                    output_path = args.image_dump_dir + "/" + args.basename + "_prev-import-grayscale-" + page_ext;
                    lo_previous.write(output_path);

                    output_path = args.image_dump_dir + "/" + args.basename + "_prev-import-overlay-" + page_ext;
                    lo_previous_diff.write(output_path);
                }
                if (args.ms_previous)
                {
                    output_path = args.image_dump_dir + "/" + args.basename + "_prev-export-grayscale-" + page_ext;
                    ms_conv_previous.write(output_path);

                    output_path = args.image_dump_dir + "/" + args.basename + "_prev-export-overlay-" + page_ext;
                    ms_conv_previous_diff.write(output_path);
                }
            }

            write_stats_to_csv(base, lo, lo_diff, lo_previous_diff, i + 1, args.basename, (csv_filename + "-import-statistics.csv"), args.lo_previous);
            write_stats_to_csv(base, ms_conv, ms_conv_diff, ms_conv_previous_diff, i + 1, args.basename, (csv_filename + "-export-statistics.csv"), args.ms_previous);

            // for debugging
            // std::string filter_path = args.import_dir + "/" + args.basename + "_import-vertical-edges" + page_ext;
            // BMP::write_with_filter(lo, filter_path, lo.get_vertical_edge_mask());

            // filter_path = args.import_dir + "/" + args.basename + "_import-blurred-edges" + page_ext;
            // BMP::write_with_filter(lo, filter_path, lo.get_blurred_edge_mask());

            // filter_path = args.import_dir + "/" + args.basename + "_origin-vertical-edges" + page_ext;
            // BMP::write_with_filter(base, filter_path, base.get_vertical_edge_mask());
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
