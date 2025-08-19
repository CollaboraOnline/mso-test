//
//
// Copyright the mso-test contributors
//
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <bit>
#include <cmath>
#include <fstream>

#include "bmp.hpp"

struct BMPColourHeader
{
    std::uint32_t red_mask;
    std::uint32_t green_mask;
    std::uint32_t blue_mask;
    std::uint32_t alpha_mask;
    std::uint32_t colour_space;
    std::uint32_t unused[16];
};

const static BMPColourHeader colour_header = {
    0x00ff0000,
    0x0000ff00,
    0x000000ff,
    0xff000000,
    0x73524742,
    {}
};

BMP::BMP(std::string filename)
{
    read(filename);

    m_blurred_edge_mask = blur_edge_mask();
    m_filtered_edge_mask = filter_long_vertical_edge_runs(10);
}

BMP::BMP(): BMP(0, 0) {}


// To be used in tests
BMP::BMP(int width, int height)
{
    if (width < 0 || height < 0)
    {
        throw std::runtime_error("The image width and height values must positive");
    }

    m_file_header.file_type = 0x4D42;
    m_file_header.placeholder_1 = 0;
    m_file_header.placeholder_2 = 0;
    m_file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColourHeader);
    m_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColourHeader);
    m_info_header.width = width;
    m_info_header.height = height;
    m_info_header.planes = 1;
    m_info_header.bit_count = 32;
    m_info_header.compression = 3;
    m_info_header.x_per_meter = 0;
    m_info_header.y_per_meter = 0;
    m_info_header.colours_used = 0;
    m_info_header.colours_important = 0;

    int row_stride = width * pixel_stride;
    m_data.resize(row_stride * height, 0);

    m_info_header.size_image = m_data.size();
    m_file_header.file_size = m_file_header.offset_data + m_data.size();

    m_blurred_edge_mask = blur_edge_mask();
    m_filtered_edge_mask = filter_long_vertical_edge_runs(10);
}

void BMP::read(std::string filename)
{
    static_assert(std::endian::native == std::endian::little, "This code only works for little endian");
    std::ifstream input{filename, std::ios_base::binary};
    if (!input)
    {
        throw std::runtime_error(std::string("Can't open the BMP file: ") + filename);
    }

    input.read(reinterpret_cast<char *>(&m_file_header), sizeof(m_file_header)); // read file header data into struct
    if (!input)
    {
        throw std::runtime_error(std::string("Error: reading file header has led to bad input state"));
    }
    if (m_file_header.file_type != 0x4D42)
    {
        throw std::runtime_error("Not a BMP file, file header type has to be 'BM'");
    }

    input.read(reinterpret_cast<char *>(&m_info_header), sizeof(m_info_header)); // read info header data into struct
    if (!input)
    {
        throw std::runtime_error(std::string("Error: reading info header has led to bad input state"));
    }
    if (m_info_header.bit_count != 32)
    {
        throw std::runtime_error("Needs to be in RGBA format (32 bits), nothing else");
    }
    if (m_info_header.height < 0)
    {
        throw std::runtime_error("The program can treat only BMP images with the origin in the bottom left corner!");
    }

    input.seekg(m_file_header.offset_data, input.beg);

    std::int32_t row_stride = m_info_header.width * m_info_header.bit_count / 8;
    std::uint32_t alligned_stride = (row_stride + 3) & ~3; // This rounds up to the nearest multiple of 4
    size_t padding_size = alligned_stride - row_stride;

    m_data.resize(row_stride * m_info_header.height);

    // read the pixel data row by row, and handle the padding if its necessary
    for (int y = 0; y < m_info_header.height; y++)
    {
        input.read(reinterpret_cast<char *>(m_data.data() + y * row_stride), row_stride);
        input.seekg(padding_size, input.cur);
    }
}

void BMP::write(std::string filename)
{
    std::ofstream output{filename, std::ios_base::binary};
    if (!output)
    {
        throw std::runtime_error("Cannot open/create the file to write");
    }

    // write the headers
    output.write(reinterpret_cast<char *>(&m_file_header), sizeof(m_file_header));
    output.write(reinterpret_cast<char *>(&m_info_header), sizeof(m_info_header));
    output.write(reinterpret_cast<const char *>(&colour_header), sizeof(colour_header));

    size_t row_stride = m_info_header.width * m_info_header.bit_count / 8;
    size_t alligned_stride = (row_stride + 3) & ~3;
    size_t padding_size = alligned_stride - row_stride;

    std::vector<std::uint8_t> padding(padding_size, 0);

    // write the pixel data row by row
    for (int y = 0; y < m_info_header.height; y++)
    {
        output.write(reinterpret_cast<char *>(m_data.data() + y * row_stride), row_stride);
        if (padding_size > 0)
        {
            output.write(reinterpret_cast<char *>(padding.data()), padding_size);
        }
    }
}

void BMP::write_with_filter(const BMP &base, std::string filename, std::vector<bool> filter_mask)
{
    const int base_width = base.get_width();
    const int base_height = base.get_height();
    BMP copy(base);
    std::vector<uint8_t> copy_data = base.get_data();
    for (int y = 0; y < base_height; y++)
    {
        for (int x = 0; x < base_width; x++)
        {
            int index = y * base_width + x;
            int byte_index = index * pixel_stride;

            if (filter_mask[index])
            {
                copy_data[byte_index + 0] = 0;
                copy_data[byte_index + 1] = 0;
                copy_data[byte_index + 2] = 255;
                copy_data[byte_index + 3] = 255;
            }
        }
    }
    copy.set_data(copy_data);
    copy.write(filename);
}

BMP BMP::stamp_name(const BMP& base, const BMP &stamp)
{
    const int base_width = base.get_width();
    const int base_height = base.get_height();
    const int stamp_width = stamp.get_width();
    const int stamp_height = stamp.get_height();

    BMP copy(base);
    std::vector<uint8_t> copy_data = copy.get_data();
    const auto &stamp_data = stamp.get_data();
    const std::size_t stamp_row_stride = stamp_width * pixel_stride;
    const std::size_t base_row_stride = base_width * pixel_stride;


    if (stamp_width > base_width || stamp_height > base_height)
    {
        throw std::runtime_error("Stamp is larger than base image");
    }

    for (int y = 0; y < stamp_height; ++y)
    {
        for (int x = 0; x < stamp_width; ++x)
        {
            std::size_t stamp_index = (stamp_height - 1 - y) * stamp_row_stride + x * pixel_stride;
            std::size_t base_index = (base_height - 1 - y) * base_row_stride + x * pixel_stride;

            for (int b = 0; b < pixel_stride; ++b)
            {
                copy_data[base_index + b] = stamp_data[stamp_index + b];
            }
        }
    }
    copy.set_data(copy_data);
    return copy;
}

void BMP::write_side_by_side(const BMP &diff, const BMP &base, const BMP &target, std::string stamp_location, std::string filename)
{
    if (diff.get_height() != base.get_height() || base.get_height() != target.get_height() ||
        diff.get_width() != base.get_width() || base.get_width() != target.get_width())
    {
        std::cerr << "Warning: Image dimensions don't match. Skipping side-by-side output for " << filename << std::endl;
        std::cerr << "Diff: " << diff.get_width() << "x" << diff.get_height() << std::endl;
        std::cerr << "Base: " << base.get_width() << "x" << base.get_height() << std::endl;
        std::cerr << "Target: " << target.get_width() << "x" << target.get_height() << std::endl;
        return;
    }

    std::ofstream output{filename, std::ios_base::binary};
    if (!output)
    {
        throw std::runtime_error("Cannot open/create the file to write");
    }

    int height = diff.get_height();
    int bit_count = diff.m_info_header.bit_count;
    int bytes_per_pixel = bit_count / 8;

    int combined_width = diff.get_width() + base.get_width() + target.get_width();
    std::size_t row_stride = combined_width * bytes_per_pixel;
    std::size_t alligned_stride = (row_stride + 3) & ~3; // rounds down to the nearest 4 (4 bytes per pixel in a 32-bit RGBA BMP)
    std::size_t padding_size = alligned_stride - row_stride;

    std::vector<std::uint8_t> combined_data(alligned_stride * height, 0);

    // stamping/labelling the images for better differentiation
    std::string diff_location = stamp_location + "/diff.bmp";
    std::string ms_office_location = stamp_location + "/ms-office.bmp";
    std::string cool_location = stamp_location + "/cool.bmp";

    BMP diff_stamp(diff_location);
    BMP ms_office_stamp(ms_office_location);
    BMP cool_stamp(cool_location);

    BMP diff_copy = BMP::stamp_name(diff, diff_stamp);
    BMP base_copy = BMP::stamp_name(base, ms_office_stamp);
    BMP target_copy = BMP::stamp_name(target, cool_stamp);

    for (int y = 0; y < height; ++y)
    {
        std::uint8_t *dest_row = &combined_data[y * alligned_stride];
        std::size_t dest_col = 0;

        int src_width = diff.get_width();
        std::size_t src_row_stride = src_width * bytes_per_pixel;
        const std::vector<std::uint8_t> &src_data = diff_copy.get_data();
        const std::uint8_t *src_row = &src_data[y * src_row_stride];

        for (int x = 0; x < src_width; ++x)
        {
            for (int b = 0; b < bytes_per_pixel; ++b)
            {
                dest_row[dest_col * bytes_per_pixel + b] = src_row[x * bytes_per_pixel + b];
            }
            dest_col++;
        }

        src_width = base.get_width();
        src_row_stride = src_width * bytes_per_pixel;
        const std::vector<std::uint8_t> &base_data = base_copy.get_data();
        const std::uint8_t *base_row = &base_data[y * src_row_stride];

        for (int x = 0; x < src_width; ++x)
        {
            for (int b = 0; b < bytes_per_pixel; ++b)
            {
                dest_row[dest_col * bytes_per_pixel + b] = base_row[x * bytes_per_pixel + b];
            }
            dest_col++;
        }

        src_width = base.get_width();
        src_row_stride = src_width * bytes_per_pixel;
        const std::vector<std::uint8_t> &target_data = target_copy.get_data();
        const std::uint8_t *target_row = &target_data[y * src_row_stride];

        for (int x = 0; x < src_width; ++x)
        {
            for (int b = 0; b < bytes_per_pixel; ++b)
            {
                dest_row[dest_col * bytes_per_pixel + b] = target_row[x * bytes_per_pixel + b];
            }
            dest_col++;
        }
    }

    BMPFileHeader file_header = diff.m_file_header;
    BMPInfoHeader info_header = diff.m_info_header;

    int image_size = alligned_stride * diff.get_height(); // alligned stride is width in bytes
    info_header.width = combined_width;
    file_header.file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColourHeader) + image_size;

    output.write(reinterpret_cast<char *>(&file_header), sizeof(file_header));
    output.write(reinterpret_cast<char *>(&info_header), sizeof(info_header));
    output.write(reinterpret_cast<const char *>(&colour_header), sizeof(colour_header));

    std::vector<std::uint8_t> padding(padding_size, 0);
    for (int y = 0; y < info_header.height; y++)
    {
        output.write(reinterpret_cast<char *>(combined_data.data() + y * row_stride), row_stride);
        if (padding_size > 0)
        {
            output.write(reinterpret_cast<char *>(padding.data()), padding_size);
        }
    }
}

int BMP::get_non_background_pixel_count() const
{
    int background_value = get_background_value();
    int non_background_count = 0;
    std::size_t pixel_count = get_width() * get_height();

    if (pixel_count == 0) return 0;
    for (std::size_t i = 0; i < pixel_count; i++)
    {
        std::uint8_t gray_value = m_data[i * pixel_stride]; // Assuming 32-bit BMP, gray value is in the first byte

        if (std::abs(gray_value - background_value) > 8)
        {
            non_background_count++;
        }
    }
    return non_background_count;
}

int BMP::get_background_value() const
{
    int total_gray = 0;
    std::int32_t stride = m_info_header.bit_count / 8;
    std::size_t pixel_count = get_width() * get_height();
    if (pixel_count == 0) return 0;
    for (std::size_t i = 0; i < pixel_count; i++)
    {
        std::uint8_t gray = m_data[i * stride];
        total_gray += gray;
    }

    return total_gray / pixel_count;
}

void BMP::set_data(std::vector<std::uint8_t> &new_data)
{
    if (new_data.size() != m_data.size())
    {
        throw std::runtime_error("New data size " + std::to_string(new_data.size()) +
                                 " differs to current data size " + std::to_string(m_data.size()));
    }
    m_data = new_data;
}

std::vector<bool> BMP::blur_edge_mask() const
{
    std::vector<bool> edge_map = sobel_edges<245>();
    std::int32_t width = m_info_header.width;
    std::int32_t height = m_info_header.height;
    std::vector<bool> blurred_mask(width * height, false);

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            int index = y * width + x;

            // If the current pixel is not an edge, skip it
            if (!edge_map[index])
                continue;

            blur_pixels<2>(x, y, width, height, blurred_mask);
        }
    }
    return blurred_mask;
}

template <int Threshold>
std::vector<bool> BMP::sobel_edges() const
{
    std::int32_t width = m_info_header.width;
    std::int32_t height = m_info_header.height;
    const std::vector<std::uint8_t> &data = m_data;

    std::vector<bool> result(width * height, false);

    // Loop through each pixel in the image, skipping the edges to avoid out-of-bounds access
    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            auto [g_x, g_y] = get_sobel_gradients(y, x, data, width);

            // Calculate gradient magnitude (clamped to 255)
            int magnitude = std::min(255, static_cast<int>(std::sqrt(g_x * g_x + g_y * g_y)));

            result[y * width + x] = (magnitude >= Threshold);
        }
    }
    return result;
}

std::vector<bool> BMP::filter_long_vertical_edge_runs(int min_run_length) const
{
    std::vector<bool> vertical_edges = get_vertical_edges<245>();
    std::vector<bool> result(vertical_edges.size(), false);
    int width = m_info_header.width;
    int height = m_info_header.height;

    for (int x = 0; x < width; x++)
    {
        int run_start = -1;
        int run_length = 0;

        for (int y = 0; y < height; y++)
        {
            int index = y * width + x;
            if (vertical_edges[index])
            {
                if (run_start == -1)
                {
                    run_start = y;
                }
                run_length++;
            }
            else
            {
                if (run_length >= min_run_length)
                {
                    // copy the run
                    for (int i = run_start; i < run_start + run_length; i++)
                    {
                        result[i * width + x] = true;
                    }
                }
                run_start = -1;
                run_length = 0;
            }
        }
    }
    return result;
}

template <int Threshold>
std::vector<bool> BMP::get_vertical_edges() const
{
    std::int32_t width = m_info_header.width;
    std::int32_t height = m_info_header.height;
    const std::vector<std::uint8_t> &data = m_data;

    std::vector<bool> result(width * height, false);

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            auto [g_x, g_y] = get_sobel_gradients(y, x, data, width);

            int abs_gx = std::abs(g_x);

            result[y * width + x] = (abs_gx >= Threshold);
        }
    }
    return result;
}

template <int Radius>
void BMP::blur_pixels(int x, int y, int width, int height, std::vector<bool> &mask)
{
    for (int dy = -Radius; dy <= Radius; dy++)
    {
        for (int dx = -Radius; dx <= Radius; dx++)
        {
            int new_x = x + dx;
            int new_y = y + dy;

            // Check if the new coordinates are within bounds
            if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height)
            {
                mask[new_y * width + new_x] = true;
            }
        }
    }
}

std::array<int, 2> BMP::get_sobel_gradients(int y, int x, const std::vector<std::uint8_t> &data, int width)
{
    auto index = [&](int row, int col)
    {
        return (row * width + col) * pixel_stride;
    };

    int top_left = data[index(y - 1, x - 1)];
    int top_mid = data[index(y - 1, x)];
    int top_right = data[index(y - 1, x + 1)];
    int middle_left = data[index(y, x - 1)];
    int middle_right = data[index(y, x + 1)];
    int bottom_left = data[index(y + 1, x - 1)];
    int bottom_mid = data[index(y + 1, x)];
    int bottom_right = data[index(y + 1, x + 1)];

    int g_x = (-1 * top_left) + (-2 * middle_left) + (-1 * bottom_left) +
              (top_right) + (2 * middle_right) + (bottom_right);
    int g_y = (top_left) + (2 * top_mid) + (top_right) +
              (-1 * bottom_left) + (-2 * bottom_mid) + (-1 * bottom_right);

    return {g_x, g_y};
}

int BMP::calculate_colour_count(const BMP& base, Colour to_compare)
{
    const int base_width = base.get_width();
    const int base_height = base.get_height();
    const std::vector<uint8_t> &base_data = base.get_data();
    int colour_count = 0;
    for (int y = 0; y < base_height; y++)
    {
        for (int x = 0; x < base_width; x++)
        {
            int index = (y * base_width + x) * pixel_stride;
            const std::uint8_t *base_row = &base_data[index];
            PixelValues base_pixel = Pixel::get_bgra(base_row);

            if (base_pixel == colour_to_pixel[to_compare])
            {
                colour_count++;
            }
        }
    }
    return colour_count;
}

const std::vector<bool>& BMP::get_blurred_edge_mask() const {
    return m_blurred_edge_mask;
}
const std::vector<bool> BMP::get_vertical_edge_mask() const {
    return get_vertical_edges<245>();
}

const std::vector<bool>& BMP::get_filtered_vertical_edge_mask() const {
    return m_filtered_edge_mask;
}

const std::vector<bool> BMP::get_sobel_edge_mask() const {
    return sobel_edges<245>();
}

void BMP::recalculate_masks()
{
    m_blurred_edge_mask = blur_edge_mask();
    m_filtered_edge_mask = filter_long_vertical_edge_runs(10);
}
