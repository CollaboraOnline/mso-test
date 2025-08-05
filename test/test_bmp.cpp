#include <catch2/catch.hpp>
#include "../src/bmp.hpp"

#include <bit>
#include <iostream>

TEST_CASE("BMP's read functionality") {
    std::string bmp_path;
    SECTION("opening and reading solid_white.bmp") {
        bmp_path = "test_data/solid_white.bmp";
        BMP solid_white;

        REQUIRE_NOTHROW(solid_white.read(bmp_path.c_str()));
    }

    SECTION("opening and reading solid_black.bmp") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black;

        REQUIRE_NOTHROW(solid_black.read(bmp_path.c_str()));
    }

    SECTION("opening and reading white_black.bmp") {
        bmp_path = "test_data/white_black.bmp";
        BMP black_white;

        REQUIRE_NOTHROW(black_white.read(bmp_path.c_str()));
    }

    SECTION("opening and reading vertical_edges.bmp") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges;

        REQUIRE_NOTHROW(vertical_edges.read(bmp_path.c_str()));
    }

    SECTION("opening and reading edges.bmp") {
        bmp_path = "test_data/edges.bmp";
        BMP edges;

        REQUIRE_NOTHROW(edges.read(bmp_path.c_str()));
    }

    SECTION("opening and reading not_bmp.png") {
        bmp_path = "test_data/not_bmp.png";
        BMP not_bmp;

        REQUIRE_THROWS_WITH(not_bmp.read(bmp_path.c_str()), Catch::Contains("Not a BMP") && Catch::Contains("BM"));
    }

    SECTION("opening and reading 24_bit.bmp") {
        bmp_path = "test_data/24_bit.bmp";
        BMP bad_image;

        REQUIRE_THROWS_WITH(bad_image.read(bmp_path.c_str()), Catch::Contains("32 bits") && Catch::Contains("RGBA"));
    }
}

TEST_CASE("BMP write functionality") {
    std::string bmp_path;
    bmp_path = "test_data/output/100x100.bmp";
    BMP dummy_image(100, 100);

    SECTION("creating a 100x100 bmp and write it out") {
        REQUIRE_NOTHROW(dummy_image.write(bmp_path.c_str()));
    }

    SECTION("writing a 100x100 bmp and reading it back in to check equivalence") {
        dummy_image.write(bmp_path.c_str());

        BMP dummy_image_read(bmp_path.c_str());
        REQUIRE(dummy_image_read.get_width() == dummy_image.get_width());
        REQUIRE(dummy_image_read.get_height() == dummy_image.get_height());
        REQUIRE(dummy_image_read.get_data() == dummy_image.get_data());
    }
}

TEST_CASE("Extracting average colour from BMP's") {
    std::string bmp_path;
    SECTION("extracting solid whites average colour") {
        bmp_path = "test_data/solid_white.bmp";
        BMP solid_white (bmp_path.c_str());

        // Solid white, means average colour should be 255
        REQUIRE(solid_white.get_background_value() == 255);
    }

    SECTION("extracting solid black average colour") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        // Solid black, means average colour should be 0
        REQUIRE(solid_black.get_background_value() == 0);
    }

    SECTION("extracting white_black average colour") {
        bmp_path = "test_data/white_black.bmp";
        BMP white_black (bmp_path.c_str());

        // half white and black, means avg is 127
        REQUIRE(white_black.get_background_value() == 127);
    }
}
