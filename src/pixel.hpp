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

#include <iostream>
#include <vector>

// Pixel structure to represent a pixel in an image
// It contains RGBA values and methods to compare pixels and convert to string
struct Pixel {
  uint8_t blue {0};
  uint8_t green {0};
  uint8_t red {0};
  uint8_t alpha {0};

  // Constructor to initialise a pixel with RGBA values from the bmp data vector
  static Pixel get_pixel(const std::vector<uint8_t>& data, int index) {
    return Pixel{data[index], data[index + 1], data[index + 2], data[index + 3]};
  }

  std::string to_string() const {
    return "R:" + std::to_string(static_cast<int>(red)) + " " +
           "G:" + std::to_string(static_cast<int>(green)) + " " +
           "B:" + std::to_string(static_cast<int>(blue)) + " " +
           "A:" + std::to_string(static_cast<int>(alpha));
  }

  // Compare this pixel with another pixel and return true if they differ
  bool differs_from(const Pixel& other, bool near_edge, int threshold = 40) {
    int avg_diff = (std::abs(red - other.red) + std::abs(green - other.green) + std::abs(blue - other.blue)) / 3;
    threshold = near_edge ? 250 : threshold; // If a pixel is near an edge, the difference between pixels is stricter
    return avg_diff > threshold;
  }

  // assumes grey scale (r == g == b), so red is good enough
  bool is_near_white(int white_threshold = 240) const {
    return red > white_threshold;
  }
};
#endif
