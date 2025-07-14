#ifndef BMP_HPP
#define BMP_HPP

#include <vector>
#include <iostream>
#include <fstream>

struct BMPInfoHeader {
  uint32_t size {0};
  int32_t width {0};
  int32_t height {0};
  int32_t bit_count {0};
};

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
  int get_average_grey() const;

  void increase_red_count(int count_increase);
  void increase_yellow_count(int count_increase);

  void print_stats();

  int channels;

private:
  BMPInfoHeader info_header;
  int red_count;
  int yellow_count;
  std::vector<uint8_t> data;
};
#endif
