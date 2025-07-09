CXX = g++
CXXFLAGS = -std=c++17 -Wall -g
SRC_DIR = src
OBJ_DIR = obj
TARGET = pixelbasher

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

# pixelbasher: obj/pixelbasher.o
# 	g++ -g obj/pixelbasher.o -o pixelbasher

# obj/pixelbasher.o: src/pixelbasher.cpp
# 	g++ -g -c src/pixelbasher.cpp -o obj/pixelbasher.o

# clean:
# 	rm -f obj/*.o pixelbasher