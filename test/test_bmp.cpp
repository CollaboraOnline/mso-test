#include <catch2/catch.hpp>
#include "../src/bmp.hpp"

#include <bit>
#include <iostream>

TEST_CASE("Read BMP files" , "[bmp][read]") {
    std::string bmp_path;
    SECTION("read solid_white.bmp succesfully") {
        bmp_path = "test_data/solid_white.bmp";
        BMP solid_white;

        REQUIRE_NOTHROW(solid_white.read(bmp_path.c_str()));
    }

    SECTION("read solid_black.bmp succesfully") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black;

        REQUIRE_NOTHROW(solid_black.read(bmp_path.c_str()));
    }

    SECTION("read white_black.bmp succesfully") {
        bmp_path = "test_data/white_black.bmp";
        BMP black_white;

        REQUIRE_NOTHROW(black_white.read(bmp_path.c_str()));
    }

    SECTION("read vertical_edges.bmp succesfully") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges;

        REQUIRE_NOTHROW(vertical_edges.read(bmp_path.c_str()));
    }

    SECTION("read edges.bmp succesfully") {
        bmp_path = "test_data/edges.bmp";
        BMP edges;

        REQUIRE_NOTHROW(edges.read(bmp_path.c_str()));
    }

    SECTION("throws error when reading non-BMP file") {
        bmp_path = "test_data/not_bmp.png";
        BMP not_bmp;

        REQUIRE_THROWS_WITH(not_bmp.read(bmp_path.c_str()), Catch::Contains("Not a BMP") && Catch::Contains("BM"));
    }

    SECTION("throws error when reading 24-bit BMP") {
        bmp_path = "test_data/24_bit.bmp";
        BMP bad_image;

        REQUIRE_THROWS_WITH(bad_image.read(bmp_path.c_str()), Catch::Contains("32 bits") && Catch::Contains("RGBA"));
    }
}

TEST_CASE("Writing BMP files", "[bmp][write]") {
    std::string bmp_path;
    bmp_path = "test_data/output/100x100.bmp";
    BMP dummy_image(100, 100);

    SECTION("creating and writes a 100x100 BMP succesfully") {
        REQUIRE_NOTHROW(dummy_image.write(bmp_path.c_str()));
    }

    SECTION("writes and re-reads 100x100 BMP with matching data") {
        dummy_image.write(bmp_path.c_str());

        BMP dummy_image_read(bmp_path.c_str());
        REQUIRE(dummy_image_read.get_width() == dummy_image.get_width());
        REQUIRE(dummy_image_read.get_height() == dummy_image.get_height());
        REQUIRE(dummy_image_read.get_data() == dummy_image.get_data());
    }
}

TEST_CASE("Computing average colour in BMP images", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("returns 255 for solid_white.bmp") {
        bmp_path = "test_data/solid_white.bmp";
        BMP solid_white (bmp_path.c_str());

        REQUIRE(solid_white.get_background_value() == 255);
    }

    SECTION("returns 0 for solid_black.bmp") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        REQUIRE(solid_black.get_background_value() == 0);
    }

    SECTION("returns 127 for white_black.bmp") {
        bmp_path = "test_data/white_black.bmp";
        BMP white_black (bmp_path.c_str());

        REQUIRE(white_black.get_background_value() == 127);
    }
}
