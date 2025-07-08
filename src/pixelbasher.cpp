#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cmath>

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

struct Pixel {
  uint8_t blue {0};
  uint8_t green {0};
  uint8_t red {0};
  uint8_t alpha {0};

  std::string toString() const {
    return "R:" + std::to_string(static_cast<int>(red)) + " " +
           "G:" + std::to_string(static_cast<int>(green)) + " " +
           "B:" + std::to_string(static_cast<int>(blue)) + " " +
           "A:" + std::to_string(static_cast<int>(alpha)) + "\n";
  }
};

class BMP {
public:
  BMPFileHeader file_header;
  BMPInfoHeader info_header;
  BMPColourHeader colour_header;
  std::vector<uint8_t> data;

  BMP(const char *filename) {
    read(filename);
  }

  void read(const char *filename) {
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

  // BMP(int32_t width, int32_t height, bool has_alpha = true) {

  // }

  void write(const char *filename) {
		std::ofstream of {filename, std::ios::binary};
		if (of) {
			if (info_header.bit_count == 32) {
				write_headers_and_data(of);
			}
		} // add a else if for the 24 bit calculation
  }

  void compareToBmp(BMP &inputBmp) {
    int32_t min_width = std::min(this->info_header.width, inputBmp.info_header.width);
    int32_t min_height = std::min(this->info_header.height, inputBmp.info_header.height);
    int32_t pixel_stride = 4;

    for (int y = 0; y < min_height; y++) {
      for (int x = 0; x < min_width; x++) {
        int currentIndex = (y * this->info_header.width + x) * pixel_stride;
        int inputIndex = (y * inputBmp.info_header.width + x) * pixel_stride;

        Pixel p1 = {this->data[currentIndex], this->data[currentIndex + 1], this->data[currentIndex + 2], this->data[currentIndex + 3]}; // BGRA
        Pixel p2 = {inputBmp.data[inputIndex], inputBmp.data[inputIndex + 1], inputBmp.data[inputIndex + 2], inputBmp.data[inputIndex + 3]};

        if (isDifference(p1, p2)) {
          this->data[currentIndex] = 0;
          this->data[currentIndex + 1] = 0;
          this->data[currentIndex + 2] = 255;
          this->data[currentIndex + 3] = 255;
        }
      }
    }
  }

  std::vector<uint8_t> sobleEdges() {
    int32_t width = info_header.width;
    int32_t height = info_header.height;
    int32_t pixel_stride = 4;

    std::vector<uint8_t> result(width * height, 0);
    for (int y = 1; y < height - 1; y++) {
      for (int x = 1; x < width - 1; x++) {
        int currentIndex = (y * this->info_header.width + x) * pixel_stride;


        uint8_t grey_scale_value = this->data[currentIndex]; // image is greyscaled so r,g,b all the same
        // top left  * - 1 +  middle left * -2 + bottom left * -1 + top right * 1 + middle right * 2 + bottom right * 1
        int8_t gX = -1 * this->data[((y - 1) * this->info_header.width + (x - 1)) * pixel_stride] +
                      -2 * this->data[(y * this->info_header.width +  (x - 1)) * pixel_stride] +
                      - 1* this->data[((y + 1) * this->info_header.width + (x - 1)) * pixel_stride] +
                      1 * this->data[((y - 1) * this->info_header.width + (x + 1)) * pixel_stride] +
                      2 * this->data[(y * this->info_header.width + (x + 1)) * pixel_stride] +
                      1 * this->data[((y + 1) * this->info_header.width + (x + 1)) * pixel_stride];

        int8_t gY = -1 * this->data[((y - 1) * this->info_header.width + (x - 1)) * pixel_stride] +
                      -2 * this->data[((y - 1) * this->info_header.width + x) * pixel_stride] +
                      -1 * this->data[((y - 1) * this->info_header.width + (x + 1)) * pixel_stride] +
                      1 * this->data[((y + 1) * this->info_header.width + (x - 1)) * pixel_stride] +
                      2 * this->data[((y + 1) * this->info_header.width + x) * pixel_stride] +
                      1 * this->data[((y + 1) * this->info_header.width + (x + 1)) * pixel_stride];

        uint8_t magnitude = std::min(255, static_cast<int>(std::sqrt(gX*gX + gY*gY)));

        result[y * width + x] = static_cast<uint8_t>(magnitude);
        std::cout << "magnitude: " << std::to_string(magnitude) << std::endl;
        // std::cout << "gX: " << std::to_string(gX) << " gY:" << std::to_string(gY) << std::endl;
      }
    }
    return result;
  }

  void highlightEdges(const std::vector<uint8_t>& edge_map, uint8_t threshold = 50) {
    int32_t width = info_header.width;
    int32_t height = info_header.height;
    int32_t pixel_stride = 4;

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        uint8_t magnitude = edge_map[y * width + x];
        if (magnitude > threshold) {
          int index = (y * width + x) * pixel_stride;
          this->data[index] = 255;
          this->data[index + 1] = 0;
          this->data[index + 2] = 0;
          this->data[index + 3] = 255;
        }
      }
    }
  }

private:
  uint32_t row_stride {0};

  uint32_t make_stride_alligned(uint32_t allign_stride) {
    uint32_t new_stride = row_stride;
    while (new_stride % allign_stride != 0) {
      new_stride ++;
    }
    return new_stride;
  }

	void write_headers(std::ofstream &of) {
		of.write((const char*)&file_header, sizeof(file_header));
		of.write((const char*)&info_header, sizeof(info_header));
		if (info_header.bit_count == 32) {
			of.write((const char*)&colour_header, sizeof(colour_header));
		}

	}

	void write_headers_and_data(std::ofstream &of) {
		write_headers(of);
		of.write((const char*)data.data(), data.size());
	}

	void check_colour_header(BMPColourHeader &colour_header) {
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

  bool isDifference(Pixel current, Pixel input) {
    int threshold = 4;
    int diff = 0;

    if ((current.red - input.red) - threshold >= 0 ||
        (current.green - input.green) - threshold >= 0 ||
        (current.blue - input.blue) - threshold >= 0 ||
        (current.alpha - input.alpha) - threshold >= 0) {
          return true;
    }
    return false;
  }
};

// argument format: authoritative bmp path, import bmp path, compared/diff bmp path
int main(int argc, char* argv[])
{
  const char* authoritative_path = argv[1];
  const char* import_path = argv[2];
  const char* output_path = argv[3];

  std::cout << authoritative_path << " " << import_path << " " << output_path;

  BMP auth_bmp(authoritative_path);
  BMP import_bmp(import_path);

  auth_bmp.highlightEdges(auth_bmp.sobleEdges());
  auth_bmp.write("highlighted.bmp");
  // auth_bmp.compareToBmp(import_bmp);
  // auth_bmp.write(output_path);

  // BMP bmp("input/import-png.bmp");
  // std::vector<Pixel> import_pixels;
  // std::vector<Pixel> authoritative_pixels;

  // BMP bmp_authoritative("input/authoritative-png.bmp");
	// bmp_authoritative.write("output/straight-copy.bmp");

  // for (int i = 0; i < bmp_authoritative.data.size(); i += 4) {
  //   Pixel pixel = {bmp_authoritative.data[i], bmp_authoritative.data[i + 1], bmp_authoritative.data[i + 2], bmp_authoritative.data[i +3],};
  //   authoritative_pixels.push_back(pixel);
  // }

  // int32_t min_width = std::min(bmp.info_header.width, bmp_authoritative.info_header.width);
  // int32_t min_height = std::min(bmp.info_header.height, bmp_authoritative.info_header.height);
  // int32_t pixel_stride = 4;

  // int diffCount = 0;

  // for (int y = 0; y < min_height; y++) {
  //   for (int x = 0; x < min_width; x++) {
  //     int index_a = (y * bmp.info_header.width + x) * pixel_stride;
  //     int index_b = (y * bmp_authoritative.info_header.width + x) * pixel_stride;

  //     Pixel p1 = {bmp.data[index_a], bmp.data[index_a + 1], bmp.data[index_a + 2], bmp.data[index_a + 3]}; // BGRA
  //     Pixel p2 = {bmp_authoritative.data[index_b], bmp_authoritative.data[index_b + 1], bmp_authoritative.data[index_b + 2], bmp_authoritative.data[index_b + 3]};

  //     if (p1.red != p2.red || p1.green != p2.green || p1.blue != p2.blue) {
  //       bmp_authoritative.data[index_b] = 0, bmp_authoritative.data[index_b + 1] = 0, bmp_authoritative.data[index_b + 2] = 255, bmp_authoritative.data[index_b + 3] = 255;
  //       diffCount++;
  //     }
  //   }
  // }
}
