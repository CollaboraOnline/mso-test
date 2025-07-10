#include <cmath>

#include "bmp.hpp"

BMP::BMP(const char* filename) {
  read(filename);
}

void BMP::read(const char *filename) {
  std::ifstream input {filename, std::ios_base::binary};
  if (input) {
    input.read((char*)&file_header, sizeof(file_header));

    if (file_header.file_type != 0x4D42)
      throw std::runtime_error("unrecognized file format - should be RGBA BMP (32 bits)");

    input.read((char*)&info_header, sizeof(BMPInfoHeader));

    if (info_header.bit_count == 32) {
      // The BMPColor header is used only for transparent images
      if (info_header.size >= (sizeof(BMPInfoHeader) + sizeof(BMPColourHeader))) {
        input.read((char*)&colour_header, sizeof(colour_header));
        // Check if pixel data is stored for BGRA
        check_colour_header(colour_header);
      }
    }
    // Go to pixel data location
    input.seekg(file_header.offset_data, input.beg);

    // adjust headers for output
    if (info_header.bit_count == 32) {
      info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColourHeader);
      file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColourHeader);
    } else {
      info_header.size = sizeof(BMPInfoHeader);
      file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    }
    file_header.file_size = file_header.offset_data;

    // Look at checking if BMPColor header has a bit count of 32
    data.resize(info_header.width * info_header.height * info_header.bit_count / 8);

    // check if we need to take into account row padding
    if (info_header.width % 4 == 0) {
      input.read((char*)data.data(), data.size());
      file_header.file_size += data.size();
    } else {
      row_stride = info_header.width * info_header.bit_count / 8;
      uint32_t new_stride = make_stride_alligned(4);
      std::vector<uint8_t> padding_row(new_stride - row_stride);

      for (int y = 0; y < info_header.height; y++) {
        input.read((char*)(data.data() + row_stride * y), row_stride);
        input.read((char*)padding_row.data(), padding_row.size());
      }
      file_header.file_size += data.size() + info_header.height * padding_row.size();
    }
  }
  else {
    throw std::runtime_error("Can't open the input image file");
  }
}

void BMP::write(const char *filename) {
  std::ofstream of {filename, std::ios::binary};
  if (of) {
    if (info_header.bit_count == 32) {
      write_headers_and_data(of);
    }
  } // add a else if for the 24 bit calculation
}


uint32_t BMP::make_stride_alligned(uint32_t allign_stride) {
  uint32_t new_stride = row_stride;
  while (new_stride % allign_stride != 0) {
    new_stride ++;
  }
  return new_stride;
}

void BMP::write_headers(std::ofstream &of) {
  of.write((const char*)&file_header, sizeof(file_header));
  of.write((const char*)&info_header, sizeof(info_header));
  if (info_header.bit_count == 32) {
    of.write((const char*)&colour_header, sizeof(colour_header));
  }

}

void BMP::write_headers_and_data(std::ofstream &of) {
  write_headers(of);
  of.write((const char*)data.data(), data.size());
}

void BMP::check_colour_header(BMPColourHeader &colour_header) {
  BMPColourHeader expected_colour_header;
  if (expected_colour_header.red_mask != colour_header.red_mask ||
      expected_colour_header.blue_mask != colour_header.blue_mask ||
      expected_colour_header.green_mask != colour_header.green_mask ||
      expected_colour_header.alpha_mask != colour_header.alpha_mask) {
        throw std::runtime_error("Unexpected colour mask format. The program expects pixel data to be BGRA");
  }
  if(expected_colour_header.colour_space_type != colour_header.colour_space_type) {
    std::cerr << "Actual colour space type: 0x" << std::hex << colour_header.colour_space_type << std::dec << std::endl;
    throw std::runtime_error("Unexpected colour space type. Expected sRGB values");
  }
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

BMPInfoHeader* BMP::get_info_header() {
  return &info_header;
}


