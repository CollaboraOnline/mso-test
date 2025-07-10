#include "pixelbasher.hpp"
#include <cmath>
#include <algorithm>

std::vector<bool> PixelBasher::sobelEdges(const BMP& bmp, int threshold) {
  int32_t width = bmp.get_width();
  int32_t height = bmp.get_height();
  const std::vector<uint8_t>& data = bmp.get_data();
  int32_t pixel_stride = 4;

  std::vector<bool> result(width * height, false);

  for (int y = 1; y < height - 1; y++) {
    for (int x = 1; x < width - 1; x++) {

      // Sobel Gx
      int gX = -1 * data[((y - 1) * width + (x - 1)) * pixel_stride]
               - 2 * data[(y * width + (x - 1)) * pixel_stride]
               - 1 * data[((y + 1) * width + (x - 1)) * pixel_stride]
               + 1 * data[((y - 1) * width + (x + 1)) * pixel_stride]
               + 2 * data[(y * width + (x + 1)) * pixel_stride]
               + 1 * data[((y + 1) * width + (x + 1)) * pixel_stride];

      // Sobel Gy
      int gY = -1 * data[((y - 1) * width + (x - 1)) * pixel_stride]
               - 2 * data[((y - 1) * width + x) * pixel_stride]
               - 1 * data[((y - 1) * width + (x + 1)) * pixel_stride]
               + 1 * data[((y + 1) * width + (x - 1)) * pixel_stride]
               + 2 * data[((y + 1) * width + x) * pixel_stride]
               + 1 * data[((y + 1) * width + (x + 1)) * pixel_stride];

      // Calculate gradient magnitude (clamped to 255)
      int magnitude = std::min(255, static_cast<int>(std::sqrt(gX * gX + gY * gY)));

      result[y * width + x] = (magnitude >= threshold);
    }
  }

  return result;
}

std::vector<bool> PixelBasher::blurEdgeMask(const BMP& bmp, const std::vector<bool>& edge_map) {
  int32_t width = bmp.get_width();
  int32_t height = bmp.get_height();
  std::vector<bool> blurred_mask(width * height, false);

  for (int y = 1; y < height - 1; y++) {
      for (int x = 1; x < width - 1; x++) {
          int index = y * width + x;

          if (!edge_map[index])
              continue;

          int radius = 3;
          for (int dy = -radius; dy <= radius; dy++) {
              for (int dx = -radius; dx <= radius; dx++) {
                  int new_x = x + dx;
                  int new_y = y + dy;

                  if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
                      blurred_mask[new_y * width + new_x] = true;
                  }
              }
          }
      }
  }
  return blurred_mask;
}

void PixelBasher::compareToBMP(BMP& base, const BMP& imported, bool enable_minor_differences) {
  int32_t min_width = std::min(base.get_width(), imported.get_width());
  int32_t min_height = std::min(base.get_height(), imported.get_height());
  int32_t pixel_stride = 4;

  std::vector<bool> auth_edge_map = sobelEdges(base);
  std::vector<bool> input_edge_map = sobelEdges(imported);

  std::vector<bool> auth_blurred_edge_mask = blurEdgeMask(base, auth_edge_map);
  std::vector<bool> input_blurred_edge_mask = blurEdgeMask(imported, input_edge_map);

  std::vector<bool> intersection_mask(min_width * min_height, false);
  for (int i = 0; i < min_width * min_height; i++) {
    intersection_mask[i] = auth_blurred_edge_mask[i] && input_blurred_edge_mask[i];
  }

  std::vector<uint8_t> new_data(min_height * min_width * pixel_stride);
  for (int y = 0; y < min_height; y++) {
    for (int x = 0; x < min_width; x++) {
      int currentIndex = (y * min_width + x) * pixel_stride;
      int inputIndex = (y * min_width + x) * pixel_stride;
      int maskIndex = y * min_width + x;

      Pixel p1 = {base.get_data()[currentIndex], base.get_data()[currentIndex + 1], base.get_data()[currentIndex + 2], base.get_data()[currentIndex + 3]}; // BGRA
      Pixel p2 = {imported.get_data()[inputIndex], imported.get_data()[inputIndex + 1], imported.get_data()[inputIndex + 2], imported.get_data()[inputIndex + 3]};

      std::vector<uint8_t> bgra = {base.get_data()[currentIndex], base.get_data()[currentIndex + 1], base.get_data()[currentIndex + 2], base.get_data()[currentIndex + 3]};
      // instead of just not highlighting any text differences, maybe pass into differs_from and set a higher threshold it has to pass and if so make the values yellow
      if (p1.differs_from(p2, intersection_mask[maskIndex])) {
        if (intersection_mask[maskIndex] && enable_minor_differences) {
          bgra = colourPixel(Colour::YELLOW);
        } else {
          bgra = colourPixel(Colour::RED);
        }
      }

      for (int i = 0; i < pixel_stride; i++) {
        new_data[currentIndex + i] = bgra[i];
      }
    }
  }
  base.get_data() = new_data;
}

std::vector<uint8_t> PixelBasher::colourPixel(Colour colour) {
  if (colour == Colour::YELLOW) {
    return {0, 197, 255, 255};
  } else {
    return {0, 0, 255, 255};
  }
}

