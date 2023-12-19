CXX=g++
CXXFLAGS=-std=c++11 -Wall
TARGET=build/macbootflash_test

all: $(TARGET)

$(TARGET): macbootflash-cpp.cpp hexfile.cpp segment.cpp tests.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $(TARGET) macbootflash-cpp.cpp hexfile.cpp segment.cpp tests.cpp

clean:
	rm -rf build
