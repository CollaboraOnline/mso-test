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

TEST_CASE("Computer the number of pixels in background", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("returns no pixels in background for solid_white.bmp succesfully") {
        bmp_path = "test_data/solid_white.bmp";
        BMP solid_white (bmp_path.c_str());

        REQUIRE(solid_white.get_non_background_count() == 0);
    }

    SECTION("returns no pixels in background for solid_black.bmp succesfully") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        REQUIRE(solid_black.get_non_background_count() == 0);
    }

    // half white and half black means the background value is 127, and pixels are either 0 or 255
    SECTION("returns all pixels in background for white_black.bmp succesfully") {
        bmp_path = "test_data/white_black.bmp";
        BMP white_black (bmp_path.c_str());
        int image_size = white_black.get_width() * white_black.get_height();

        REQUIRE(white_black.get_non_background_count() == image_size);
    }
}

TEST_CASE("setting the pixel data", "[bmp][set_data]") {
    SECTION("throws error when new data doesn't mach original data size") {
        BMP dummy_image(100, 100);
        std::vector<uint8_t> new_data((100 * pixel_stride) * 101, 0);

        REQUIRE_THROWS_WITH(dummy_image.set_data(new_data), Catch::Contains("differs"));
    }

    SECTION("sets dummy data to new_data succesfully") {
        BMP dummy_image(100, 100);
        std::vector<uint8_t> new_data((100 * pixel_stride) * 100, 0);

        REQUIRE_NOTHROW(dummy_image.set_data(new_data));
    }
}

TEST_CASE("running sobel edge detection", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("returns no edges for solid_black.bmp") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = solid_black.get_sobel_edge_mask();
        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) == 0);
    }

    SECTION("returns that edges exist for edges.bmp") {
        bmp_path = "test_data/edges.bmp";
        BMP edges (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = edges.get_sobel_edge_mask();
        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) > 0);
    }

    SECTION("returns that edges exist for vertical_edges.bmp") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = vertical_edges.get_sobel_edge_mask();
        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) > 0);
    }
}

TEST_CASE("blurring the sobel edge mask", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("sobel edge count remains the same as blurred edge mask") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = solid_black.get_sobel_edge_mask();
        std::vector<bool> blurred_edge_mask = solid_black.get_blurred_edge_mask();

        int sobel_edge_count = std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true);
        int blurred_edge_count = std::count(blurred_edge_mask.begin(), blurred_edge_mask.end(), true);

        REQUIRE(blurred_edge_count == 0); // no edges in solid black image
        REQUIRE(sobel_edge_count == blurred_edge_count);
    }

    SECTION("returns a larger edge count for blurred edge mask than sobel edge mask") {
        bmp_path = "test_data/edges.bmp";
        BMP edges (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = edges.get_sobel_edge_mask();
        std::vector<bool> blurred_edge_mask = edges.get_blurred_edge_mask();

        int sobel_edge_count = std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true);
        int blurred_edge_count = std::count(blurred_edge_mask.begin(), blurred_edge_mask.end(), true);

        REQUIRE(blurred_edge_count != 0);
        REQUIRE(blurred_edge_count > sobel_edge_count);
    }
}

TEST_CASE("running vertical edge detection", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("returns no vertical edges for solid_black.bmp") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        std::vector<bool> vertical_edge_mask = solid_black.get_vertical_edge_mask();
        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) == 0);
    }

    SECTION("returns that vertical edges exits for vertical_edges.bmp") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges (bmp_path.c_str());

        std::vector<bool> vertical_edge_mask = vertical_edges.get_vertical_edge_mask();
        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) > 0);
    }
}

TEST_CASE("running filtered vertical edge detection", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("filtered vertical edges returns the same as vertical edges") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        std::vector<bool> vertical_edge_mask = solid_black.get_vertical_edge_mask();
        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) == 0);
    }

    SECTION("returns a smaller amount of vertical edges compared to total edges") {
        bmp_path = "test_data/edges.bmp";
        BMP edges (bmp_path.c_str());

        std::vector<bool> vertical_edge_mask = edges.get_vertical_edge_mask();
        std::vector<bool> blurred_edge_mask = edges.get_blurred_edge_mask();

        int vertical_edge_count = std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true);
        int blurred_edge_count = std::count(blurred_edge_mask.begin(), blurred_edge_mask.end(), true);

        REQUIRE(blurred_edge_count > vertical_edge_count);
    }


    SECTION("returns a postive filtered edge count") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges (bmp_path.c_str());

        std::vector<bool> filtered_edge_mask = vertical_edges.get_blurred_edge_mask();
        int filtered_edge_count = std::count(filtered_edge_mask.begin(), filtered_edge_mask.end(), true);

        REQUIRE(filtered_edge_count > 0);
    }
}

