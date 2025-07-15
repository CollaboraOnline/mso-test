#ifndef BMP_HPP
#define BMP_HPP

#include <vector>
#include <iostream>
#include <fstream>

// BMP info file header structure, doesn't contain every detail, just the necessary ones
struct BMPInfoHeader {
  uint32_t size {0};
  int32_t width {0};
  int32_t height {0};
  int32_t bit_count {0};
};

// BMP class to handle BMP image reading, writing, and manipulation
class BMP {
public:
  BMP(const char* filename);
  void read(const char* filename);
  void write(const char* filename);

  // Getters for BMP data
  const std::vector<uint8_t>& get_data() const;
  std::vector<uint8_t>& get_data();
  int get_width() const;
  int get_height() const;
  int get_red_count() const;
  int get_yellow_count() const;
  int get_average_grey() const;
  const BMPInfoHeader& get_info_header() const { return info_header; }

  // Methods to manipulate BMP data
  void increase_red_count(int count_increase);
  void increase_yellow_count(int count_increase);
  void set_data(const std::vector<uint8_t>& new_data);

  // Prints out statistics about the BMP image
  void print_stats();

private:
  BMPInfoHeader info_header;
  int red_count = 0;
  int yellow_count = 0;
  int channels = 0;
  std::vector<uint8_t> data;
};
#endif
