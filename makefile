CXX=g++
CXXFLAGS=-std=c++11 -Wall
TARGET=build/mcbootflash_test

all: $(TARGET)

$(TARGET): mcbootflash-cpp.cpp hexfile.cpp segment.cpp tests.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $(TARGET) mcbootflash-cpp.cpp hexfile.cpp segment.cpp tests.cpp

clean:
	rm -rf build
