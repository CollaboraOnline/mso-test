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
    BMP(std::string filename);
    BMP(int width, int height);
    BMP();

    void write(std::string filename);
    static BMP stamp_name(const BMP &base, const BMP &stamp);
    static void write_side_by_side(const BMP &diff, const BMP &base, const BMP &target, std::string stamp_location, std::string filename);
    static int calculate_colour_count(const BMP& base, Colour to_compare);
    static void write_with_filter(const BMP &base, std::string filename, std::vector<bool> filter_mask);

    const std::vector<std::uint8_t> &get_data() const { return m_data; }
    const std::vector<bool>& get_blurred_edge_mask() const;
    const std::vector<bool> get_vertical_edge_mask() const;
    const std::vector<bool>& get_filtered_vertical_edge_mask() const;
    const std::vector<bool> get_sobel_edge_mask() const;
    int get_width() const { return m_info_header.width; }
    int get_height() const { return m_info_header.height; }
    int get_red_count() const { return m_red_count; }
    int get_yellow_count() const { return m_yellow_count; }
    int get_blue_count() const { return m_blue_count; }
    int get_green_count() const { return m_green_count; }
    int get_background_value() const;
    int get_non_background_pixel_count() const;

    void increment_red_count(int new_red) { m_red_count += new_red; }
    void increment_yellow_count(int new_yellow) { m_yellow_count += new_yellow; }
    void increment_blue_count(int new_blue) { m_blue_count += new_blue; }
    void increment_green_count(int new_green) { m_green_count += new_green; }
    void set_data(std::vector<std::uint8_t> &new_data);

private:
    void read(std::string filename);

    void recalculate_masks();
    void set_data_internal(std::vector<std::uint8_t> &new_data);

    template <int Threshold>
    std::vector<bool> sobel_edges() const;
    template <int Threshold> // compile-time constant
    std::vector<bool> get_vertical_edges() const;
    template <int Radius> // compile-time constant
    static void blur_pixels(int x, int y, int width, int height, std::vector<bool> &mask);

    std::vector<bool> blur_edge_mask() const;
    std::vector<bool> filter_long_vertical_edge_runs(int min_run_length)const ;
    static std::array<int, 2> get_sobel_gradients(int y, int x, const std::vector<std::uint8_t> &data, int width);

    BMPFileHeader m_file_header;
    BMPInfoHeader m_info_header;
    std::vector<std::uint8_t> m_data;
    int m_red_count = 0;
    int m_yellow_count = 0;
    int m_blue_count = 0;
    int m_green_count = 0;
    std::vector<bool> m_blurred_edge_mask;
    std::vector<bool> m_filtered_edge_mask;
};
#endif
