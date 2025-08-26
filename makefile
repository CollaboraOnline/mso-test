CXX = g++
CXXFLAGS = -std=c++20 -Wall -g -MMD -MP -mavx2 -O3
SRC_DIR = src
OBJ_DIR = obj
TARGET = pixelbasher

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

.PHONY: check clean
check: $(TARGET)
	rm -f converted/import/doc/* converted/export/doc/*

	mkdir -p ./converted/import/doc ./converted/export/doc

	./$(TARGET) fdo69695-1.doc \
		converted/input/authoritative-page-0.bmp \
		converted/input/import-page-0.bmp \
		converted/input/export-page-0.bmp \
		converted/import/doc \
		converted/export/doc \
		converted/import-compare/doc \
		converted/export-compare/doc \
		converted/image-dump/doc \
		stamps \
		false false false false false

	cmp  ./converted/import/doc/fdo69695-1.doc_import-1.bmp ./converted/expected/fdo69695-1.doc_import-1.bmp
	cmp  ./converted/export/doc/fdo69695-1.doc_export-1.bmp ./converted/expected/fdo69695-1.doc_export-1.bmp

clean:
	rm -fr $(OBJ_DIR) \
		$(TARGET) \
		./*.csv \
		converted/export \
		converted/export-compare \
		converted/import \
		converted/import-compare
