//
//
// Copyright the mso-test contributors
//
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef PIXELBASHER_HPP
#define PIXELBASHER_HPP

#include <array>
#include <cstdint>
#include <vector>

#include "bmp.hpp"
#include "pixel.hpp"

// PixelBasher class to handle the comparison of BMP images and generate diff images
class PixelBasher
{
public:
    // Compares two BMP images and generates a diff image based on the differences (the diff is applied to the base image)
    static BMP compare_bmps(const BMP &original, const BMP &target, bool enable_minor_differences);
    static BMP compare_regressions(const BMP &original, const BMP &current, BMP &previous);

private:
    static std::vector<bool> get_intersection_mask(const BMP &original, const BMP &target, int min_width, int min_height);
    static PixelValues compare_pixel_regression(BMP& diff, PixelValues original, PixelValues current, PixelValues previous);
    static PixelValues compare_pixels(PixelValues original, PixelValues target, BMP &diff, int original_background_value, bool near_edge, bool vertical_edge, bool minor_differences);
    static PixelValues colour_pixel(Colour colour);
};
#endif
