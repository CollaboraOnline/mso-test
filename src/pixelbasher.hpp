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

#include <vector>
#include <array>

#include "bmp.hpp"

// PixelBasher class to handle the comparison of BMP images and generate diff images
class PixelBasher {
public:
  enum Colour {
    RED,
    YELLOW,
  };

  // Sobel edge detection algorithm to find edges in the BMP image
  std::vector<bool> sobel_edges(BMP& bmp, int threshold = 15);

  // Applies a blur effect to the edges found by the Sobel algorithm by a given radius
  std::vector<bool> blur_edge_mask(const BMP& bmp, const std::vector<bool>& edge_map);

  // Compares two BMP images and generates a diff image based on the differences (the diff is applied to the base image)
  BMP compare_to_bmp(BMP& base, BMP& imported, bool enable_minor_differences);

  // Generates a rgba pixel from a Colour enum
  std::array<uint8_t, 4> colour_pixel(Colour colour);
 };
#endif
