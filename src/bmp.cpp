#include <cmath>
#include "bmp.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

BMP::BMP(const char* filename) {
  read(filename);
}

void BMP::read(const char* filename) {
  int width, height, channels;
  unsigned char* img_data = stbi_load(filename, &width, &height, &channels, 4);

  if (!img_data) {
    throw std::runtime_error("Failed to load BMP image");
  }

  info_header.width = width;
  info_header.height = height;
  info_header.bit_count = 32; // Assuming 32-bit BMP
  this->channels = channels;
  data.assign(img_data, img_data + width * height * 4);

  stbi_image_free(img_data);
}

void BMP::write(const char* filename) {
   int success = stbi_write_bmp(filename, info_header.width, info_header.height, channels, data.data());
    if (!success) {
        throw std::runtime_error("Failed to write BMP image");
    }
}

int BMP::get_average_grey() const {
  int total_grey = 0;
  size_t pixel_count = get_width() * get_height();
  for (size_t i = 0; i < pixel_count; i++) {
    uint8_t gray = data[i * 4];
    // Calculate average grey value
    total_grey += gray;
  }

  return total_grey / pixel_count;
}

std::vector<uint8_t>& BMP::get_data(){
  return data;
}

const std::vector<uint8_t>& BMP::get_data() const {
    return data;
}

int BMP::get_width() const {
  return info_header.width;
}

int BMP::get_height() const {
  return info_header.height;
}

int BMP::get_red_count() const {
  return red_count;
}

int BMP::get_yellow_count() const {
  return yellow_count;
}

BMPInfoHeader* BMP::get_info_header() {
  return &info_header;
}

void BMP::increase_red_count(int count_increase) {
  red_count += count_increase;
}

void BMP::increase_yellow_count(int count_increase) {
  yellow_count += count_increase;
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
