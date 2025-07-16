//
//
// Copyright the mso-test contributors
//
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "bmp.hpp"

#include <fstream>

BMP::BMP(const char* filename) {
  read(filename);
}

void BMP::read(const char* filename) {
  std::ifstream input {filename, std::ios_base::binary};

  if (!input) {
    throw std::runtime_error("Can't open the BMP image file");
  }

  input.read((char*)&file_header, sizeof(file_header)); // read file header data into struct
  if (file_header.file_type != 0x4D42) {
    throw std::runtime_error("Not a BMP file");
  }

  input.read((char*)&info_header, sizeof(info_header)); // read info header data into struct
  if (info_header.bit_count != 32) {
    throw std::runtime_error("Need to be in RGBA format, nothing else");
  }
  if (info_header.height < 0) {
    throw std::runtime_error("The program can treat only BMP images with the origin in the bottom left corner!");
}
  
  input.seekg(file_header.offset_data, input.beg);

  info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColourHeader); // we know that its a 32-bit BMP
  file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColourHeader);
  file_header.file_size = file_header.offset_data;
  
  row_stride = info_header.width * info_header.bit_count / 8;
  uint32_t alligned_stride = (row_stride + 3) & ~3; // This rounds up to the nearest multiple of 4
  size_t padding_size = alligned_stride - row_stride;
  
  std::vector<uint8_t> padding(padding_size);
  data.resize(row_stride * info_header.height);

  // read the pixel data row by row, and handle the padding if its necessary
  for (int y = 0; y < info_header.height; y++) {
    //uint8_t* row_ptr = data.data() + y * row_stride;
    input.read((char*)(data.data() + y * row_stride), row_stride);

    if (padding_size > 0) {
      input.read((char*)padding.data(), padding_size); // read in the padding so next read is correct
    }
  }
  file_header.file_size += data.size() + padding_size * info_header.height;
}
  

  
void BMP::write(const char* filename) {
  std::ofstream output{filename, std::ios_base::binary};
  if (!output) {
    throw std::runtime_error("Cannot open file for writing");
  }
  
  // write the headers
  output.write((const char*)&file_header, sizeof(BMPFileHeader));
  output.write((const char*)&info_header, sizeof(BMPInfoHeader));
  output.write((const char*)&colour_header, sizeof(BMPColourHeader));

  size_t row_stride = info_header.width * info_header.bit_count / 8;
  size_t alligned_stride = (row_stride + 3) & ~3;
  size_t padding_size = alligned_stride - row_stride;

  std::vector<uint8_t> padding(padding_size, 0);

  // write the pixel data row by row
  for (int y = 0; y < info_header.height; y++) {
    //uint8_t* row_ptr = data.data() + y * row_stride;
    output.write((const char*)(data.data() + y * row_stride), row_stride);
    if (padding_size > 0) {
      output.write((const char*)padding.data(), padding_size);
    }
  }

  output.close();
}

int BMP::get_average_grey() const {
  int total_grey = 0;
  int32_t stride = 4;
  size_t pixel_count = get_width() * get_height();
  for (size_t i = 0; i < pixel_count; i++) {
    uint8_t gray = data[i * stride];
    // Calculate average grey value
    total_grey += gray;
  }

  return total_grey / pixel_count;
}

void BMP::set_data(std::vector<uint8_t>& new_data) {
  if (new_data.size() != data.size()) {
    throw std::runtime_error("New data size does not match existing data size");
  }
  data = new_data;
}

void BMP::print_stats() {
  size_t total_pixels = static_cast<size_t>(get_width() * get_height());
  double red_percentage = (static_cast<double>(red_count) / total_pixels) * 100;
  double yellow_percentage = (static_cast<double>(yellow_count) / total_pixels) * 100;
  double red_yellow_percentage = (static_cast<double>(red_count + yellow_count) / total_pixels) * 100;

  std::cout << "Total Pixels = " << total_pixels << " | " << "Red Pixels = " << red_count <<
    " | " << "Yellow Pixels = " << yellow_count << "\n" << "Red Percentage = " << red_percentage << "% | "
    << "Yellow Percentage = " << yellow_percentage << "% | " << "Red & Yellow Percentage = " << red_yellow_percentage
    << "%";
}
