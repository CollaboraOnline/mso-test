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
#include <cmath>

#include "pixel.hpp"
#include "pixelbasher.hpp"

BMP PixelBasher::compare_bmps(const BMP &original, const BMP &target, bool enable_minor_differences)
{
    int min_width = std::min(original.get_width(), target.get_width());
    int min_height = std::min(original.get_height(), target.get_height());

    BMP diff(original);

    std::vector<bool> intersection_mask = get_intersection_mask(original, target, min_width, min_height);
    std::vector<bool> original_filtered_vertical_edges = original.get_filtered_vertical_edge_mask();
    std::vector<bool> target_filtered_vertical_edges = target.get_filtered_vertical_edge_mask();

    int original_background_value = original.get_background_value();

    // The diff data is based on the base image, so we create a new data vector to hold the modified pixels
    auto &original_data = original.get_data();
    auto &target_data = target.get_data();
    std::vector<std::uint8_t> diff_data = original.get_data();

    int original_width = original.get_width();
    int target_width = target.get_width();
    // Loops through the min width and height, if size of images differ slightly.
    for (int y = 0; y < min_height; y++)
    {
        for (int x = 0; x < min_width; x++)
        {
            // The intersection mask is based on height and width not stride, so we calculate it separately
            int original_index = (y * original_width + x) * pixel_stride;
            int target_index = (y * target_width + x) * pixel_stride;
            int mask_index = y * min_width + x;

            const std::uint8_t *original_row = &original_data[original_index];
            const std::uint8_t *target_row = &target_data[target_index];

            PixelValues original_pixel = Pixel::get_bgra(original_row);
            PixelValues target_pixel = Pixel::get_bgra(target_row);

            bool near_edge = intersection_mask[mask_index];
            bool vertical_edge = false;

            if (original_filtered_vertical_edges[mask_index] || target_filtered_vertical_edges[mask_index])
            {
                vertical_edge = true;
            }

            PixelValues bgra = compare_pixels(original_pixel, target_pixel, diff, original_background_value, near_edge, vertical_edge, enable_minor_differences);

            for (int i = 0; i < pixel_stride; i++)
            {
                diff_data[original_index + i] = bgra[i];
            }
        }
    }
    diff.set_data(diff_data);
    return diff;
}

BMP PixelBasher::compare_regressions(const BMP &original, const BMP &current, BMP &previous)
{
    std::int32_t min_width = std::min(original.get_width(), current.get_width());
    std::int32_t min_height = std::min(original.get_height(), current.get_height());

    BMP diff(original);

    // The diff data is based on the base image, so we create a new data vector to hold the modified pixels
    std::vector<std::uint8_t> diff_data = original.get_data();
    auto &current_data = current.get_data();
    auto &previous_data = previous.get_data();

    std::int32_t original_width = original.get_width();
    std::int32_t current_width = current.get_width();
    std::int32_t previous_width = previous.get_width();
    for (int y = 0; y < min_height; y++)
    {
        for (int x = 0; x < min_width; x++)
        {
            int original_index = (y * original_width + x) * pixel_stride;
            int current_index = (y * current_width + x) * pixel_stride;
            int previous_index = (y * previous_width + x) * pixel_stride;

            const std::uint8_t *original_row = &diff_data[original_index];
            const std::uint8_t *current_row = &current_data[current_index];
            const std::uint8_t *previous_row = &previous_data[previous_index];

            PixelValues original_pixel = Pixel::get_bgra(original_row);
            PixelValues current_pixel = Pixel::get_bgra(current_row);
            PixelValues previous_pixel = Pixel::get_bgra(previous_row);

            PixelValues bgra = compare_pixel_regression(original_pixel, current_pixel, previous_pixel);

            for (int i = 0; i < pixel_stride; i++)
            {
                diff_data[original_index + i] = bgra[i];
            }
        }
    }
    diff.set_data(diff_data);
    return diff;
}

std::vector<bool> PixelBasher::get_intersection_mask(const BMP &original, const BMP &target, int width, int height)
{

    std::vector<bool> original_edge_map = original.get_blurred_edge_mask();
    std::vector<bool> target_edge_map = target.get_blurred_edge_mask();

    std::vector<bool> intersection_mask(width * height, false);
    for (int i = 0; i < width * height; i++)
    {
        intersection_mask[i] = original_edge_map[i] && target_edge_map[i];
    }
    return intersection_mask;
}

PixelValues PixelBasher::compare_pixels(PixelValues original, PixelValues target, BMP &diff, int original_background_value, bool near_edge, bool vertical_edge, bool minor_differences)
{
    const bool differs = Pixel::differs_from(original, target, near_edge, original_background_value);

    if (!differs)
    {
        if (minor_differences && near_edge && Pixel::differs_from(original, target, original_background_value, false))
        {
            diff.increment_red_count(1);
            return colour_to_pixel[Colour::YELLOW];
        }
        return original;
    }

    if (vertical_edge)
    {
        diff.increment_yellow_count(1);
        return colour_to_pixel[Colour::DARK_YELLOW];
    }

    if (near_edge) {
        return original;
    }

    diff.increment_red_count(1);
    return colour_to_pixel[Colour::RED];
}

PixelValues PixelBasher::compare_pixel_regression(PixelValues original, PixelValues current, PixelValues previous)
{
    bool current_is_red = Pixel::is_red(current);
    bool previous_is_red = Pixel::is_red(previous);

    if (current_is_red && previous_is_red)
    {
        return colour_to_pixel[Colour::BLUE];
    }
    if (current_is_red && !previous_is_red)
    {
        return colour_to_pixel[Colour::RED]; // a regression
    }
    if (!current_is_red && previous_is_red)
    {
        return colour_to_pixel[Colour::GREEN]; // a fix
    }
    return original;
}