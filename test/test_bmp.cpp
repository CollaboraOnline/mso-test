#include <catch2/catch.hpp>
#include "../src/bmp.hpp"

#include <iostream>

TEST_CASE("some test") {
    std::string sw_path = "test_data/solid_white.bmp";
    BMP solid_white(sw_path.c_str());
}
