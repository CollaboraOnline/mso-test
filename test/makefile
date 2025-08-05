SRC_DIR = .
OBJ_DIR = obj
TARGET = unittest

CXX = g++
CXXFLAGS = -std=c++20 -Wall -g -MMD -MP -I/usr/local/include -I../src
Catch2_CFLAGS = $(shell pkg-config --cflags catch2)
Catch2_LDFLAGS = $(shell pkg-config --libs catch2)

TEST_SRCS = $(wildcard $(SRC_DIR)/*.cpp)

SRC_FILES = ../src/bmp.cpp ../src/pixelbasher.cpp

SRCS = $(TEST_SRCS) $(SRC_FILES)

OBJS = $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))

$(OBJ_DIR)/%.o:
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(Catch2_CFLAGS) -c $(filter %/$*.cpp, $(SRCS)) -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(Catch2_CFLAGS) $(OBJS) -o $(TARGET) $(Catch2_LDFLAGS)

-include $(OBJS:.o=.d)
.PHONY: clean check

check: $(TARGET)
	./$(TARGET)

clean:
	rm -fr $(OBJ_DIR) $(TARGET)
