SRC_DIR = .
OBJ_DIR = obj
TARGET = unittest

CXX = g++
CXXFLAGS = -std=c++20 -Wall -g -MMD -MP -I../src -mavx2
Catch2_CFLAGS = $(shell pkg-config --cflags catch2)
Catch2_LDFLAGS = $(shell pkg-config --libs catch2-with-main)

TEST_SRCS = $(wildcard $(SRC_DIR)/*.cpp)
SRC_FILES = ../src/bmp.cpp ../src/pixelbasher.cpp

SRCS = $(TEST_SRCS) $(SRC_FILES)

OBJS = $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))

# Rule for cpp files in current directory
$(OBJ_DIR)/%.o: %.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(Catch2_CFLAGS) -c $< -o $@

# Rule for cpp files in src directory
$(OBJ_DIR)/%.o: ../src/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(Catch2_CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(Catch2_CFLAGS) $(OBJS) -o $(TARGET) $(Catch2_LDFLAGS)

-include $(OBJS:.o=.d)
.PHONY: clean check

check: $(TARGET)
	mkdir -p test_data/output/
	./$(TARGET)

clean:
	rm -fr $(OBJ_DIR) $(TARGET) test_data/output
