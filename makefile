
pixelbasher: obj/pixelbasher.o
	g++ -g obj/pixelbasher.o -o pixelbasher

obj/pixelbasher.o: src/pixelbasher.cpp
	g++ -g -c src/pixelbasher.cpp -o obj/pixelbasher.o

clean:
	rm -f obj/*.o pixelbasher