#include <catch2/catch_all.hpp>
#include "../src/pixel.hpp"

#include <bit>
#include <iostream>

using Catch::Matchers::ContainsSubstring;

TEST_CASE("Pixel struct functionality", "[pixel][struct]") {
    SECTION("Get BGRA values from pixel data") {
        std::uint8_t pixel_data[] = {255, 0, 0, 255};
        PixelValues pixel = Pixel::get_bgra(pixel_data);

        REQUIRE(pixel[0] == 255);
        REQUIRE(pixel[1] == 0);
        REQUIRE(pixel[2] == 0);
        REQUIRE(pixel[3] == 255);
    }

    SECTION("Check if two different pixels differ") {
        PixelValues original_blue = {255, 0, 0, 255};
        PixelValues target_green = {0, 255, 0, 255};

        bool differs = Pixel::differs_from(original_blue, target_green, 128, false);
        REQUIRE(differs == true);
    }

     SECTION("Check if two identical pixels differ") {
        PixelValues original_red = {0, 0, 255, 255};
        PixelValues target_red = {0, 0, 255, 255};

        bool differs = Pixel::differs_from(original_red, target_red, 128, false);
        REQUIRE(differs == false);
    }

    SECTION("Check if a red pixel is red") {
        PixelValues red_pixel = {0, 0, 255, 255};
        bool is_red = Pixel::is_red(red_pixel);
        REQUIRE(is_red == true);
    }

    SECTION("Check if a non-red pixel is red") {
        PixelValues blue_pixel = {255, 0, 0, 255};
        bool is_red = Pixel::is_red(blue_pixel);
        REQUIRE(is_red == false);
    }
}