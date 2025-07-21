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

#include <cstdint>
#include <cstddef>
#include <vector>
#include <iostream>

#pragma pack(push, 1)

struct BMPFileHeader {
  uint16_t file_type{0x4D42}; // BM
  uint32_t file_size{0};
  uint16_t placeholder_1{0};
  uint16_t placeholder_2{0};
  uint32_t offset_data{0}; // this is the start position of pixel data (bytes)
};

struct BMPInfoHeader {
  uint32_t size{0}; // Size of the header (bytes)
  int32_t width{0}; // in pixels
  int32_t height{0}; // in pixels
  uint16_t planes{1};
  uint16_t bit_count{0}; // useful to check if file is RGBA or RGB
  uint32_t compression{0};
  uint32_t size_image{0};
  int32_t x_per_meter{0};
  int32_t y_per_meter{0};
  uint32_t colours_used{0};
  uint32_t colours_important{0};
};

struct BMPColourHeader {
  uint32_t red_mask{0x00ff0000};
  uint32_t green_mask{0x0000ff00};
  uint32_t blue_mask {0x000000ff};
  uint32_t alpha_mask {0xff000000};
  uint32_t colour_space {0x73524742};
  uint32_t unused[16]{0};
};
#pragma pack(pop)

class BMP{
public:
  BMP(const char* filename, std::string basename);
  BMP(const BMP& other);

  void read(const char* filename);
  void write(const char* filename);

  const BMPFileHeader& get_file_header() { return file_header; }
  const BMPInfoHeader& get_info_header() { return info_header; }
  const BMPColourHeader& get_colour_header() { return colour_header; }

  const std::vector<uint8_t>& get_data() const { return data; }
  size_t get_row_stride() { return row_stride; }

  int get_width() const { return info_header.width; }
  int get_height() const { return info_header.height; }

  int get_red_count() { return red_count; }
  int get_yellow_count() { return yellow_count; }
  int get_background_value() const { return background_value; }
  int get_non_background_count() const { return non_background_count; }
  std::string get_basename() const { return basename; }

  void increment_red_count(int new_red) { red_count += new_red; }
  void increment_yellow_count(int new_yellow) { yellow_count += new_yellow; }

  void set_data(std::vector<uint8_t>& new_data);

  std::string print_stats();

private:
  int get_average_colour() const;
  int get_non_background_pixel_count(int background_value) const;

  std::string basename;

  BMPFileHeader file_header;
  BMPInfoHeader info_header;
  BMPColourHeader colour_header;

  std::vector<uint8_t> data;
  size_t row_stride;

  int red_count = 0;
  int yellow_count = 0;
  int background_value = 0; // used to determine background colour
  int non_background_count = 0;
};
#endif
