#include <catch2/catch_all.hpp>
#include "../src/bmp.hpp"

using Catch::Matchers::ContainsSubstring;

TEST_CASE("BMP Class Constructor", "[bmp][constructor]") {
    SECTION("Default constructor creates an empty BMP object") {
        BMP bmp;
        REQUIRE(bmp.get_width() == 0);
        REQUIRE(bmp.get_height() == 0);
        REQUIRE(bmp.get_data().empty());
        REQUIRE(bmp.get_red_count() == 0);
        REQUIRE(bmp.get_yellow_count() == 0);
        REQUIRE(std::count(bmp.get_blurred_edge_mask().begin(), bmp.get_blurred_edge_mask().end(), true) == 0);
        REQUIRE(std::count(bmp.get_filtered_vertical_edge_mask().begin(), bmp.get_filtered_vertical_edge_mask().end(), true) == 0);
    }

    SECTION("Constructor with width and height initializes BMP with correct dimensions") {
        BMP bmp(100, 100);
        std::vector<bool> bmp_blurred_edge_mask = bmp.get_blurred_edge_mask();
        std::vector<bool> bmp_filtered_vertical_edge_mask = bmp.get_filtered_vertical_edge_mask();
        REQUIRE(bmp.get_width() == 100);
        REQUIRE(bmp.get_height() == 100);
        REQUIRE(bmp.get_data().size() == 100 * 100 * pixel_stride);
        REQUIRE(bmp.get_red_count() == 0);
        REQUIRE(bmp.get_yellow_count() == 0);
        REQUIRE(std::count(bmp_blurred_edge_mask.begin(), bmp_blurred_edge_mask.end(), true) == 0);
        REQUIRE(std::count(bmp_filtered_vertical_edge_mask.begin(), bmp_filtered_vertical_edge_mask.end(), true) == 0);
    }
}

TEST_CASE("Read BMP files" , "[bmp][read]") {
    SECTION("Reads solid_white.bmp successfully") {
        REQUIRE_NOTHROW(BMP("test_data/solid_white.bmp"));
    }

    SECTION("Reads solid_black.bmp successfully") {
        REQUIRE_NOTHROW(BMP("test_data/solid_black.bmp"));
    }

    SECTION("Reads white_black.bmp successfully") {
        REQUIRE_NOTHROW(BMP("test_data/white_black.bmp"));
    }

    SECTION("Reads vertical_edges.bmp successfully") {
        REQUIRE_NOTHROW(BMP("test_data/vertical_edges.bmp"));
    }

    SECTION("Reads edges.bmp successfully") {
        REQUIRE_NOTHROW(BMP("test_data/edges.bmp"));
    }

    SECTION("Throws an error when reading non-BMP file") {
        REQUIRE_THROWS_WITH(BMP("test_data/not_bmp.png"), ContainsSubstring("Not a BMP"));
    }

    SECTION("Throws an error when reading 24-bit BMP") {
        REQUIRE_THROWS_WITH(BMP("test_data/24_bit.bmp"), ContainsSubstring("32 bits") && ContainsSubstring("RGBA"));
    }
}

TEST_CASE("Writing BMP files", "[bmp][write]") {
    SECTION("Create and writes a 100x100 BMP successfully") {
        BMP dummy_image(100, 100);
        REQUIRE_NOTHROW(dummy_image.write("test_data/output/write-100x100.bmp"));
    }

    SECTION("Writes and re-reads 100x100 BMP with matching data") {
        std::string bmp_path = "test_data/output/write-read-100x100.bmp";
        BMP dummy_image(100, 100);
        dummy_image.write(bmp_path);

        BMP dummy_image_input(bmp_path);

        REQUIRE(dummy_image_input.get_width() == 100);
        REQUIRE(dummy_image_input.get_height() == 100);
        REQUIRE(dummy_image_input.get_width() == dummy_image.get_width());
        REQUIRE(dummy_image_input.get_height() == dummy_image.get_height());
        REQUIRE(dummy_image_input.get_data() == dummy_image.get_data());
    }
}

TEST_CASE("Writing filters & masks onto BMP files", "[bmp][write]") {
    SECTION("Creates and writes a BMP with an edge_mask successfully") {
        BMP dummy_image(100, 100);
        std::vector<bool> edge_mask (100 * 100, true);

        REQUIRE_NOTHROW(BMP::write_with_filter(dummy_image, "test_data/output/write-filter.bmp", edge_mask));
    }

    SECTION("Writes and re-reads a BMP with all red due to edge mask") {
        std::string bmp_path = "test_data/output/write-filter-read.bmp";
        BMP dummy_image(100, 100);
        int dummy_image_red_count = BMP::calculate_colour_count(dummy_image, Colour::RED);

        std::vector<bool> edge_mask (100 * 100, true);
        BMP::write_with_filter(dummy_image, bmp_path, edge_mask);

        BMP dummy_image_input(bmp_path);
        int dummy_image_input_red_count = BMP::calculate_colour_count(dummy_image_input, Colour::RED);

        REQUIRE(dummy_image_red_count == 0);
        REQUIRE(dummy_image_input_red_count == (int)edge_mask.size());
    }
}

TEST_CASE("Writing stamps to BMP files", "[bmp][write]") {
    std::string cool_stamp_path = "../stamps/cool.bmp";
    SECTION("Creates and writes a BMP with a stamp successfully") {
        BMP dummy_image(100, 100);
        BMP cool_stamp(cool_stamp_path);
        BMP stamped;

        REQUIRE_NOTHROW(stamped = BMP::stamp_name(dummy_image, cool_stamp));
        REQUIRE_NOTHROW(stamped.write("test_data/output/write-stamp.bmp"));
    }

    SECTION("Throws an error when image is smaller than the stamp") {
        BMP dummy_image(20, 20);
        BMP cool_stamp(cool_stamp_path);

        REQUIRE_THROWS_WITH(BMP::stamp_name(dummy_image, cool_stamp), ContainsSubstring("Stamp is larger"));
    }

    SECTION("Writes and re-reads a stamped bmp") {
        std::string bmp_path = "test_data/output/write-stamp-read.bmp";
        BMP dummy_image(100, 100);
        int dummy_image_non_bg_count = dummy_image.get_non_background_pixel_count();

        BMP cool_stamp(cool_stamp_path);
        BMP dummy_image_stamped = BMP::stamp_name(dummy_image, cool_stamp);
        dummy_image_stamped.write(bmp_path);

        BMP dummy_image_input(bmp_path);
        int dummy_image_input_non_bg_count = dummy_image_input.get_non_background_pixel_count();

        REQUIRE(dummy_image_input_non_bg_count > dummy_image_non_bg_count);
    }
}

TEST_CASE("Computing average colour in BMP images", "[bmp][pixel-analysis]") {
    SECTION("Returns 255 for solid_white.bmp") {
        BMP solid_white ("test_data/solid_white.bmp");

        REQUIRE(solid_white.get_background_value() == 255);
    }

    SECTION("Returns 0 for solid_black.bmp") {
        BMP solid_black ("test_data/solid_black.bmp");

        REQUIRE(solid_black.get_background_value() == 0);
    }

    SECTION("Returns 127 for white_black.bmp") {
        BMP white_black ("test_data/white_black.bmp");

        REQUIRE(white_black.get_background_value() == 127);
    }
}

TEST_CASE("Compute the number of pixels in background", "[bmp][pixel-analysis]") {
    SECTION("Returns no pixels background pixels for solid_white.bmp successfully") {
        BMP solid_white ("test_data/solid_white.bmp");

        REQUIRE(solid_white.get_non_background_pixel_count() == 0);
    }

    SECTION("Returns no background pixels for solid_black.bmp successfully") {
        BMP solid_black ("test_data/solid_black.bmp");

        REQUIRE(solid_black.get_non_background_pixel_count() == 0);
    }

    // half white and half black means the background value is 127, and pixels are either 0 or 255
    SECTION("Returns that all pixels are part of the background in white_black.bmp successfully") {
        BMP white_black ("test_data/white_black.bmp");
        int image_size = white_black.get_width() * white_black.get_height();

        REQUIRE(white_black.get_non_background_pixel_count() == image_size);
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
    SECTION("Returns no edges for solid_black.bmp") {
        BMP solid_black ("test_data/solid_black.bmp");
        std::vector<bool> sobel_edge_mask = solid_black.get_sobel_edge_mask();

        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) == 0);
    }

    SECTION("Returns that edges exist for edges.bmp") {
        BMP edges ("test_data/edges.bmp");
        std::vector<bool> sobel_edge_mask = edges.get_sobel_edge_mask();

        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) > 0);
    }

    SECTION("Returns that edges exist for vertical_edges.bmp") {
        BMP vertical_edges ("test_data/vertical_edges.bmp");
        std::vector<bool> sobel_edge_mask = vertical_edges.get_sobel_edge_mask();

        REQUIRE(std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true) > 0);
    }
}

TEST_CASE("Blurring the sobel edge mask", "[bmp][pixel-analysis]") {
    SECTION("Sobel edge count remains the same as blurred edge mask") {
        BMP solid_black ("test_data/solid_black.bmp");

        std::vector<bool> sobel_edge_mask = solid_black.get_sobel_edge_mask();
        std::vector<bool> blurred_edge_mask = solid_black.get_blurred_edge_mask();

        int sobel_edge_count = std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true);
        int blurred_edge_count = std::count(blurred_edge_mask.begin(), blurred_edge_mask.end(), true);

        REQUIRE(blurred_edge_count == 0); // no edges in solid black image
        REQUIRE(sobel_edge_count == blurred_edge_count);
    }

    SECTION("Returns a larger edge count for blurred edge mask than sobel edge mask") {
        BMP edges ("test_data/edges.bmp");

        std::vector<bool> sobel_edge_mask = edges.get_sobel_edge_mask();
        std::vector<bool> blurred_edge_mask = edges.get_blurred_edge_mask();

        int sobel_edge_count = std::count(sobel_edge_mask.begin(), sobel_edge_mask.end(), true);
        int blurred_edge_count = std::count(blurred_edge_mask.begin(), blurred_edge_mask.end(), true);

        REQUIRE(blurred_edge_count != 0);
        REQUIRE(blurred_edge_count > sobel_edge_count);
    }
}

TEST_CASE("Running vertical edge detection", "[bmp][pixel-analysis]") {
    SECTION("Returns no vertical edges for solid_black.bmp") {
        BMP solid_black ("test_data/solid_black.bmp");
        std::vector<bool> vertical_edge_mask = solid_black.get_vertical_edge_mask();

        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) == 0);
    }

    SECTION("Returns that vertical edges exist for vertical_edges.bmp") {
        BMP vertical_edges ("test_data/vertical_edges.bmp");
        std::vector<bool> vertical_edge_mask = vertical_edges.get_vertical_edge_mask();

        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) > 0);
    }
}

TEST_CASE("Running filtered vertical edge detection", "[bmp][pixel-analysis]") {
    SECTION("filtered vertical edges returns the same as vertical edges") {
        BMP solid_black ("test_data/solid_black.bmp");
        std::vector<bool> vertical_edge_mask = solid_black.get_vertical_edge_mask();

        REQUIRE(std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true) == 0);
    }

    SECTION("Returns a smaller amount of vertical edges compared to total edges") {
        BMP edges ("test_data/edges.bmp");

        std::vector<bool> vertical_edge_mask = edges.get_vertical_edge_mask();
        std::vector<bool> blurred_edge_mask = edges.get_blurred_edge_mask();

        int vertical_edge_count = std::count(vertical_edge_mask.begin(), vertical_edge_mask.end(), true);
        int blurred_edge_count = std::count(blurred_edge_mask.begin(), blurred_edge_mask.end(), true);

        REQUIRE(blurred_edge_count > vertical_edge_count);
    }


    SECTION("Returns a positive filtered edge count") {
        BMP vertical_edges ("test_data/vertical_edges.bmp");

        std::vector<bool> filtered_edge_mask = vertical_edges.get_blurred_edge_mask();
        int filtered_edge_count = std::count(filtered_edge_mask.begin(), filtered_edge_mask.end(), true);

        REQUIRE(filtered_edge_count > 0);
    }
}
