//
//
// Copyright the mso-test contributors
//
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef PIXEL_HPP
#define PIXEL_HPP

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

constexpr int pixel_stride = 4;
using PixelValues = std::array<std::uint8_t, pixel_stride>;

enum Colour {
    RED,
    YELLOW,
    DARK_YELLOW,
    BLUE,
    GREEN,
    COLOUR_COUNT
};

static constexpr PixelValues colour_to_pixel[COLOUR_COUNT] = {
    {0, 0, 255, 255}, // RED
    {0, 197, 255, 255}, // YELLOW
    {0, 128, 139, 255}, // DARK_YELLOW
    {255, 0, 0, 255}, // BLUE
    {0, 255, 0, 255} // GREEN
};

struct Pixel
{
    std::uint8_t blue{0};
    std::uint8_t green{0};
    std::uint8_t red{0};
    std::uint8_t alpha{0};

    static PixelValues get_bgra(const std::uint8_t *src_row)
    {
        return {src_row[0], src_row[1], src_row[2], src_row[3]};
    }

    // Compare this pixel with another pixel and return true if they differ
    static bool differs_from(PixelValues original, PixelValues target, int background_value, bool near_edge, int threshold = 40)
    {
        int gray_original = original[0];
        int gray_target = target[0];
        int gray_diff = std::abs(gray_original - gray_target);
        int from_background = std::abs(gray_original - background_value);

        if (from_background < 15) {
            threshold += 20; // likely that it's just noise
        }

        if (near_edge) {
            threshold += 50;
        }

        return gray_diff > threshold;
    }

    static bool is_red(PixelValues pixel)
    {
        return pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 255;
    }
};
#endif
