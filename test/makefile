CXX = g++
CXXFLAGS = -std=c++20 -Wall -g -MMD -MP -I/usr/local/include
Catch2_CFLAGS = $(shell pkg-config --cflags catch2)
Catch2_LDFLAGS = $(shell pkg-config --libs catch2)

SRC_DIR = .
OBJ_DIR = obj
TARGET = unittest

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(Catch2_CFLAGS) $(OBJS) -o $(TARGET) $(Catch2_LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(Catch2_CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)
.PHONY: clean
clean:
	rm -fr $(OBJ_DIR) \
		$(TARGET)
