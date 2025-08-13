//
//
// Copyright the mso-test contributors
//
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef BMP_HPP
#define BMP_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "pixel.hpp"

#pragma pack(push, 1)
struct BMPFileHeader
{
    std::uint16_t file_type; // BM
    std::uint32_t file_size;
    std::uint16_t placeholder_1;
    std::uint16_t placeholder_2;
    std::uint32_t offset_data; // this is the start position of pixel data (bytes)
};

struct BMPInfoHeader
{
    std::uint32_t size;  // Size of the header (bytes)
    std::int32_t width;  // in pixels
    std::int32_t height; // in pixels
    std::uint16_t planes;
    std::uint16_t bit_count; // useful to check if file is RGBA or RGB
    std::uint32_t compression;
    std::uint32_t size_image;
    std::int32_t x_per_meter;
    std::int32_t y_per_meter;
    std::uint32_t colours_used;
    std::uint32_t colours_important;
};
#pragma pack(pop)

class BMP
{
public:
    BMP(const char *filename);
    BMP(int width, int height, bool has_alpha = true);
    BMP();

    void write(const char *filename);
    static BMP stamp_name(const BMP &base, const BMP &stamp);
    static void write_side_by_side(const BMP &diff, const BMP &base, const BMP &target, std::string stamp_location, const char *filename);
    void write_with_filter(const char *filename, std::vector<bool> filter_mask);
    int calculate_colour_count(Colour to_compare) const;

    const std::vector<std::uint8_t> &get_data() const { return m_data; }
    const std::vector<bool> &get_blurred_edge_mask() const { return m_blurred_edge_mask; }
    const std::vector<bool> &get_filtered_vertical_edge_mask() const { return m_filtered_vertical_edges; }
    const std::vector<bool> &get_vertical_edge_mask() const { return m_vertical_edges; }
    const std::vector<bool> &get_sobel_edge_mask() const { return m_sobel_edge_mask; }
    int get_width() const { return m_info_header.width; }
    int get_height() const { return m_info_header.height; }
    int get_red_count() const { return m_red_count; }
    int get_yellow_count() const { return m_yellow_count; }
    int get_background_value() const { return m_background_value; }
    int get_non_background_count() const { return m_non_background_count; }

    void increment_red_count(int new_red) { m_red_count += new_red; }
    void increment_yellow_count(int new_yellow) { m_yellow_count += new_yellow; }
    void set_data(std::vector<std::uint8_t> &new_data);

private:
    void read(const char *filename);

    int get_average_colour() const;
    int get_non_background_pixel_count(int background_value) const;

    template <int Threshold>
    std::vector<bool> sobel_edges();

    template <int Threshold> // compile-time constant
    std::vector<bool> get_vertical_edges();
    std::vector<bool> filter_long_vertical_edge_runs(const std::vector<bool> &vertical_edges, int min_run_length);

    static std::array<int, 2> get_sobel_gradients(int y, int x, const std::vector<std::uint8_t> &data, int width);

    std::vector<bool> blur_edge_mask(const std::vector<bool> &edge_map);

    template <int Radius> // compile-time constant
    static void blur_pixels(int x, int y, int width, int height, std::vector<bool> &mask);

    BMPFileHeader m_file_header;
    BMPInfoHeader m_info_header;

    std::vector<std::uint8_t> m_data;
    std::vector<bool> m_sobel_edge_mask;
    std::vector<bool> m_blurred_edge_mask;
    std::vector<bool> m_vertical_edges;
    std::vector<bool> m_filtered_vertical_edges;

    int m_red_count = 0;
    int m_yellow_count = 0;
    int m_background_value = 0; // used to determine background colour
    int m_non_background_count = 0;
};
#endif
