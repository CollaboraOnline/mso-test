#include <catch2/catch_all.hpp>
#include "../src/bmp.hpp"
#include "../src/pixel.hpp"

#include <bit>
#include <iostream>

using Catch::Matchers::ContainsSubstring;

TEST_CASE("Read BMP files" , "[bmp][read]") {
    std::string bmp_path;
    SECTION("Reads solid_white.bmp successfully") {
        bmp_path = "test_data/solid_white.bmp";
        BMP solid_white;

        REQUIRE_NOTHROW(solid_white.read(bmp_path.c_str()));
    }

    SECTION("Reads solid_black.bmp successfully") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black;

        REQUIRE_NOTHROW(solid_black.read(bmp_path.c_str()));
    }

    SECTION("Reads white_black.bmp successfully") {
        bmp_path = "test_data/white_black.bmp";
        BMP black_white;

        REQUIRE_NOTHROW(black_white.read(bmp_path.c_str()));
    }

    SECTION("Reads vertical_edges.bmp successfully") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges;

        REQUIRE_NOTHROW(vertical_edges.read(bmp_path.c_str()));
    }

    SECTION("Reads edges.bmp successfully") {
        bmp_path = "test_data/edges.bmp";
        BMP edges;

        REQUIRE_NOTHROW(edges.read(bmp_path.c_str()));
    }

    SECTION("Throws an error when reading non-BMP file") {
        bmp_path = "test_data/not_bmp.png";
        BMP not_bmp;
        REQUIRE_THROWS_WITH(not_bmp.read(bmp_path.c_str()), ContainsSubstring("Not a BMP"));
    }

    SECTION("Throws an error when reading 24-bit BMP") {
        bmp_path = "test_data/24_bit.bmp";
        BMP bad_image;

        REQUIRE_THROWS_WITH(bad_image.read(bmp_path.c_str()), ContainsSubstring("32 bits") && ContainsSubstring("RGBA"));
    }
}

TEST_CASE("Writing BMP files", "[bmp][write]") {
    std::string bmp_path;

    SECTION("Create and writes a 100x100 BMP successfully") {
        BMP dummy_image(100, 100);
        bmp_path = "test_data/output/write-100x100.bmp";
        REQUIRE_NOTHROW(dummy_image.write(bmp_path.c_str()));
    }

    SECTION("Writes and re-reads 100x100 BMP with matching data") {
        BMP dummy_image(100, 100);
        bmp_path = "test_data/output/write-read-100x100.bmp";
        dummy_image.write(bmp_path.c_str());

        BMP dummy_image_input(bmp_path.c_str());
        REQUIRE(dummy_image_input.get_width() == dummy_image.get_width());
        REQUIRE(dummy_image_input.get_height() == dummy_image.get_height());
        REQUIRE(dummy_image_input.get_data() == dummy_image.get_data());
    }
}

TEST_CASE("Writing filters & masks onto BMP files", "[bmp][write]") {
    SECTION("Creates and writes a BMP with an edge_mask successfully") {
        std::string bmp_path = "test_data/output/write-filter.bmp";
        BMP dummy_image(100, 100);

        std::vector<bool> edge_mask (100 * 100, true);
        REQUIRE_NOTHROW(dummy_image.write_with_filter(bmp_path.c_str(), edge_mask));
    }

    SECTION("Writes and re-reads a BMP with all red due to edge mask") {
        std::string bmp_path = "test_data/output/write-filter-read.bmp";
        BMP dummy_image(100, 100);
        int dummy_image_red_count = dummy_image.calculate_colour_count(Colour::RED);

        std::vector<bool> edge_mask (100 * 100, true);
        dummy_image.write_with_filter(bmp_path.c_str(), edge_mask);

        BMP dummy_image_input(bmp_path.c_str());
        int dummy_image_input_red_count = dummy_image_input.calculate_colour_count(Colour::RED);

        REQUIRE(dummy_image_red_count == 0);
        REQUIRE(dummy_image_input_red_count == (int)edge_mask.size());
    }
}

TEST_CASE("Writing stamps to BMP files", "[bmp][write]") {
    std::string cool_stamp_path = "../stamps/cool.bmp";
    SECTION("Creates and writes a BMP with a stamp successfully") {
        std::string bmp_path = "test_data/output/write-stamp.bmp";
        BMP dummy_image(100, 100);

        BMP cool_stamp(cool_stamp_path.c_str());

        REQUIRE_NOTHROW(dummy_image.stamp_name(cool_stamp));
    }

    SECTION("Throws an error when image is smaller than the stamp") {
        std::string bmp_path = "test_data/output/20x20.bmp";
        BMP dummy_image(20, 20);

        BMP cool_stamp(cool_stamp_path.c_str());

        REQUIRE_THROWS_WITH(dummy_image.stamp_name(cool_stamp), ContainsSubstring("Stamp is larger"));
    }

    SECTION("Writes and re-reads a stamped bmp") {
        std::string bmp_path = "test_data/output/write-stamp-read.bmp";
        BMP dummy_image(100, 100);
        int dummy_image_non_bg_count = dummy_image.get_non_background_count();

        BMP cool_stamp(cool_stamp_path.c_str());
        dummy_image.stamp_name(cool_stamp);
        dummy_image.write(bmp_path.c_str());

        BMP dummy_image_input(bmp_path.c_str());
        int dummy_image_input_non_bg_count = dummy_image_input.get_non_background_count();

        REQUIRE(dummy_image_input_non_bg_count > dummy_image_non_bg_count);
    }
}

TEST_CASE("Computing average colour in BMP images", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("Returns 255 for solid_white.bmp") {
        bmp_path = "test_data/solid_white.bmp";
        BMP solid_white (bmp_path.c_str());

        REQUIRE(solid_white.get_background_value() == 255);
    }

    SECTION("Returns 0 for solid_black.bmp") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        REQUIRE(solid_black.get_background_value() == 0);
    }

    SECTION("Returns 127 for white_black.bmp") {
        bmp_path = "test_data/white_black.bmp";
        BMP white_black (bmp_path.c_str());

        REQUIRE(white_black.get_background_value() == 127);
    }
}

TEST_CASE("Compute the number of pixels in background", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("Returns no pixels background pixels for solid_white.bmp successfully") {
        bmp_path = "test_data/solid_white.bmp";
        BMP solid_white (bmp_path.c_str());

        REQUIRE(solid_white.get_non_background_count() == 0);
    }

    SECTION("Returns no background pixels for solid_black.bmp successfully") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        REQUIRE(solid_black.get_non_background_count() == 0);
    }

    // half white and half black means the background value is 127, and pixels are either 0 or 255
    SECTION("Returns that all pixels are part of the background in white_black.bmp successfully") {
        bmp_path = "test_data/white_black.bmp";
        BMP white_black (bmp_path.c_str());
        int image_size = white_black.get_width() * white_black.get_height();

        REQUIRE(white_black.get_non_background_count() == image_size);
    }
}

TEST_CASE("Setting the pixel data", "[bmp][set_data]") {
    SECTION("Throws an error when new data doesn't match original data size") {
        BMP dummy_image(100, 100);
        std::vector<uint8_t> new_data((100 * pixel_stride) * 101, 0);

        REQUIRE_THROWS_WITH(dummy_image.set_data(new_data), ContainsSubstring("differs"));
    }

    SECTION("Sets dummy data to new_data successfully") {
        BMP dummy_image(100, 100);
        std::vector<uint8_t> new_data((100 * pixel_stride) * 100, 0);

        REQUIRE_NOTHROW(dummy_image.set_data(new_data));
    }
}

TEST_CASE("Running sobel edge detection", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("Returns no edges for solid_black.bmp") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = solid_black.get_sobel_edge_mask();
        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) == 0);
    }

    SECTION("Returns that edges exist for edges.bmp") {
        bmp_path = "test_data/edges.bmp";
        BMP edges (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = edges.get_sobel_edge_mask();
        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) > 0);
    }

    SECTION("Returns that edges exist for vertical_edges.bmp") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = vertical_edges.get_sobel_edge_mask();
        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) > 0);
    }
}

TEST_CASE("Blurring the sobel edge mask", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("Sobel edge count remains the same as blurred edge mask") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        std::vector<bool> sobel_edge_mask = solid_black.get_sobel_edge_mask();
        std::vector<bool> blurred_edge_mask = solid_black.get_blurred_edge_mask();

        int sobel_edge_count = std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true);
        int blurred_edge_count = std::count(blurred_edge_mask.begin(), blurred_edge_mask.end(), true);

        REQUIRE(blurred_edge_count == 0); // no edges in solid black image
        REQUIRE(sobel_edge_count == blurred_edge_count);
    }

    SECTION("Returns a larger edge count for blurred edge mask than sobel edge mask") {
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

TEST_CASE("Running vertical edge detection", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("Returns no vertical edges for solid_black.bmp") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        std::vector<bool> vertical_edge_mask = solid_black.get_vertical_edge_mask();
        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) == 0);
    }

    SECTION("Returns that vertical edges exist for vertical_edges.bmp") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges (bmp_path.c_str());

        std::vector<bool> vertical_edge_mask = vertical_edges.get_vertical_edge_mask();
        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) > 0);
    }
}

TEST_CASE("Running filtered vertical edge detection", "[bmp][pixel-analysis]") {
    std::string bmp_path;
    SECTION("filtered vertical edges returns the same as vertical edges") {
        bmp_path = "test_data/solid_black.bmp";
        BMP solid_black (bmp_path.c_str());

        std::vector<bool> vertical_edge_mask = solid_black.get_vertical_edge_mask();
        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) == 0);
    }

    SECTION("Returns a smaller amount of vertical edges compared to total edges") {
        bmp_path = "test_data/edges.bmp";
        BMP edges (bmp_path.c_str());

        std::vector<bool> vertical_edge_mask = edges.get_vertical_edge_mask();
        std::vector<bool> blurred_edge_mask = edges.get_blurred_edge_mask();

        int vertical_edge_count = std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true);
        int blurred_edge_count = std::count(blurred_edge_mask.begin(), blurred_edge_mask.end(), true);

        REQUIRE(blurred_edge_count > vertical_edge_count);
    }


    SECTION("Returns a positive filtered edge count") {
        bmp_path = "test_data/vertical_edges.bmp";
        BMP vertical_edges (bmp_path.c_str());

        std::vector<bool> filtered_edge_mask = vertical_edges.get_blurred_edge_mask();
        int filtered_edge_count = std::count(filtered_edge_mask.begin(), filtered_edge_mask.end(), true);

        REQUIRE(filtered_edge_count > 0);
    }
}
