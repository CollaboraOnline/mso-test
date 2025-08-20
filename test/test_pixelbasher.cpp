#include <catch2/catch_all.hpp>
#include "../src/pixelbasher.hpp"

using Catch::Matchers::ContainsSubstring;

TEST_CASE("PixelBasher compare two BMP's", "[pixelbasher][compare]") {
    SECTION("Compare two different BMPs and generate a diff image with minor differences disabled") {
        BMP original("test_data/solid_white.bmp");
        BMP target("test_data/vertical_edges.bmp");

        BMP diff = PixelBasher::compare_bmps(original, target, false);

        REQUIRE(diff.get_width() == 50); // <= original.get_width()
        REQUIRE(diff.get_height() == 50);
        REQUIRE(diff.get_red_count() > 0);
    }

    SECTION("Compare identical BMPs and generate a diff image") {
        BMP original("test_data/solid_white.bmp");
        BMP target("test_data/solid_white.bmp");

        BMP diff = PixelBasher::compare_bmps(original, target, false);

        REQUIRE(diff.get_width() == 50); // <= original.get_width()
        REQUIRE(diff.get_height() == 50);
        REQUIRE(diff.get_red_count() == 0);
    }

    SECTION("Compare two different BMPs and generate a diff image with minor differences enabled") {
        BMP original("test_data/text.bmp");
        BMP target("test_data/slightly-different-text.bmp");

        BMP diff = PixelBasher::compare_bmps(original, target, true);

        REQUIRE(diff.get_width() == 50); // <= original.get_width()
        REQUIRE(diff.get_height() == 50);
        REQUIRE(diff.get_yellow_count() > 0);
    }
}

TEST_CASE("PixelBasher compare regression", "[pixelbasher][regression]") {
    SECTION("Compare differences with regressed BMP version versus previous regressed BMP version") {
        BMP original("test_data/solid_white.bmp");
        BMP current("test_data/solid_black.bmp");
        BMP previous("test_data/vertical_edges.bmp");

        BMP current_diff = PixelBasher::compare_bmps(original, current, false);
        BMP previous_diff = PixelBasher::compare_bmps(original, previous, false);
        BMP diff = PixelBasher::compare_regressions(original, current_diff, previous_diff   );

        REQUIRE(diff.get_width() == 50); // <= original.get_width()
        REQUIRE(diff.get_height() == 50);
        REQUIRE(diff.get_red_count() > 0);
        REQUIRE(diff.get_blue_count() > 0);
    }

    SECTION("Compare differences with fixed BMP version versus regressed BMP version") {
        BMP original("test_data/solid_white.bmp");
        BMP current("test_data/solid_white.bmp");
        BMP previous("test_data/solid_black.bmp");

        BMP current_diff = PixelBasher::compare_bmps(original, current, false);
        BMP previous_diff = PixelBasher::compare_bmps(original, previous, false);
        BMP diff = PixelBasher::compare_regressions(original, current_diff, previous_diff);

        int diff_size = diff.get_width() * diff.get_height();
        REQUIRE(diff.get_width() == 50); // <= original.get_width()
        REQUIRE(diff.get_height() == 50);
        REQUIRE(diff.get_green_count() == diff_size);
        REQUIRE(diff.get_red_count() == 0);
        REQUIRE(diff.get_blue_count() == 0);
    }
}
