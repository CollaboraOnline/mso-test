#include <catch2/catch.hpp>
#include "../src/bmp.hpp"

#include <bit>
#include <iostream>

TEST_CASE("BMP's can be read") {
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
