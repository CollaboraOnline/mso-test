#ifndef BMP_HPP
#define BMP_HPP

#include <vector>
#include <iostream>
#include <fstream>

#include "pixel.hpp"

#pragma pack(push, 1)
struct BMPFileHeader {
  uint16_t file_type {0x4D42};
  uint32_t file_size {0};
  uint16_t reserved1 {0};
  uint16_t reserved2 {0};
  uint32_t offset_data {0};
};

struct BMPInfoHeader {
  uint32_t size {0};
  int32_t width {0};
  int32_t height {0};
  uint16_t planes {1};
  uint16_t bit_count {0};
  uint32_t compression {0};
  uint32_t size_image {0};
  int32_t x_pixels_per_meter {0};
  int32_t y_pixels_per_meter {0};
  uint32_t colours_used {0};
  uint32_t colours_important {0};
};

struct BMPColourHeader {
  uint32_t red_mask {0x00ff0000};
  uint32_t green_mask {0x0000ff00};
  uint32_t blue_mask {0x000000ff};
  uint32_t alpha_mask {0xff000000};
  uint32_t colour_space_type {0x73524742};
  uint32_t unused[16] {0};
};
#pragma pack(pop)

class BMP {
public:
  BMP(const char* filename);
  void read(const char* filename);
  void write(const char* filename);
  const std::vector<uint8_t>& get_data() const;
  std::vector<uint8_t>& get_data();
  int get_width() const;
  int get_height() const;
  int get_red_count() const;
  int get_yellow_count() const;
  BMPInfoHeader* get_info_header();

  void increase_red_count(int count_increase);
  void increase_yellow_count(int count_increase);

  void print_stats();
private:
  void write_headers(std::ofstream& of);
  void write_headers_and_data(std::ofstream& of);
  void check_colour_header(BMPColourHeader& colour_header);
  uint32_t make_stride_alligned(uint32_t allign_stride);

  BMPFileHeader file_header;
  BMPInfoHeader info_header;
  BMPColourHeader colour_header;
  std::vector<uint8_t> data;
  uint32_t row_stride{0};
  int red_count;
  int yellow_count;
};
#endif
