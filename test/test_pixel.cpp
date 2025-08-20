#include <catch2/catch_all.hpp>
#include "../src/pixel.hpp"

#include <bit>
#include <iostream>

using Catch::Matchers::ContainsSubstring;

TEST_CASE("Pixel struct functionality", "[pixel][struct]") {
    SECTION("Get BGRA values from pixel data") {
        std::uint8_t pixel_data[] = {50, 100, 150, 255};
        PixelValues pixel = Pixel::get_bgra(pixel_data);

        REQUIRE(pixel[0] == 50);
        REQUIRE(pixel[1] == 100);
        REQUIRE(pixel[2] == 150);
        REQUIRE(pixel[3] == 255);
    }

    SECTION("Check if two different pixels differ") {
        PixelValues original_blue = {255, 0, 0, 255}; // Blue pixel
        PixelValues target_green = {0, 255, 0, 255}; // Green pixel

        bool differs = Pixel::differs_from(original_blue, target_green, 128, false);
        REQUIRE(differs == true);
    }

     SECTION("Check if two identical pixels differ") {
        PixelValues original_red = {0, 0, 255, 255}; // Red pixel
        PixelValues target_red = {0, 0, 255, 255}; // Same red pixel

        bool differs = Pixel::differs_from(original_red, target_red, 128, false);
        REQUIRE(differs == false);
    }

    SECTION("Check if slightly different pixels differ") {
        PixelValues original_pixel = {100, 100, 100, 255};
        PixelValues target_pixel = {111, 111, 111, 255}; // Slightly different

        bool differs = Pixel::differs_from(original_pixel, target_pixel, 15, false); // threshold is 40
        REQUIRE(differs == false);
    }

    SECTION("Check if slightly different pixels differ with a lower threshold") {
        PixelValues original_pixel = {100, 100, 100, 255};
        PixelValues target_pixel = {111, 111, 111, 255}; // Slightly different

        bool differs = Pixel::differs_from(original_pixel, target_pixel, 15, false, 5);
        REQUIRE(differs == true);
    }

    SECTION("Check if pixels differ with the original pixel near the background") {
        PixelValues original_pixel = {10, 10, 10, 255}; // Near background value
        PixelValues target_pixel = {71, 71, 71, 255};

        bool differs = Pixel::differs_from(original_pixel, target_pixel, 15, false); // threshold, 40 + 20(noise)
        REQUIRE(differs == true);
    }

    SECTION("Check if pixels differ with the original pixel near background and an edge") {
        PixelValues original_pixel = {10, 10, 10, 255}; // Near background value
        PixelValues target_pixel = {100, 100, 100, 255};

        bool differs = Pixel::differs_from(original_pixel, target_pixel, 15, true); // threshold, 40 + 20(noise) + 50(edge)
        REQUIRE(differs == false);
    }

    SECTION("Check if a red pixel is red") {
        PixelValues red_pixel = {0, 0, 255, 255}; // Red pixel
        bool is_red = Pixel::is_red(red_pixel);
        REQUIRE(is_red == true);
    }

    SECTION("Check if a non-red pixel is red") {
        PixelValues blue_pixel = {255, 0, 0, 255}; // Blue pixel
        bool is_red = Pixel::is_red(blue_pixel);
        REQUIRE(is_red == false);
    }
}