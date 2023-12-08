#ifndef HEXFILE_H
#define HEXFILE_H

#include "segment.h"
#include "macbootflash-c.cpp"


// Intel hex types.
#define IHEX_DATA 0
#define IHEX_END_OF_FILE 1
#define IHEX_EXTENDED_SEGMENT_ADDRESS 2
#define IHEX_START_SEGMENT_ADDRESS 3
#define IHEX_EXTENDED_LINEAR_ADDRESS 4
#define IHEX_START_LINEAR_ADDRESS 5

std::string bytesToHexString(const std::vector<uint8_t> &bytes);

class HexFile
{
private:
    int current_segment_index; //utilisé dans addSegment (en python Segments.add) pour l'insertion rapide
    //int parce que prends la valeur -1 à l'initialisation

    //I did not copy the "_current_segment" member in python, because I can get it by its index
    unsigned int word_size_bytes;

    
    unsigned int execution_start_address;

public:
    std::vector<Segment> debug_segments; //this works
    std::vector<Segment> debug_segments_before_crop;
    std::vector<Segment> segments;
    unsigned int processed_total_bytes;
    
    HexFile();
    unsigned int crc_ihex(const std::vector<uint8_t> &bytes);

    void unpack_ihex(
        const std::string &record,
        unsigned int &type_,
        unsigned int &address,
        unsigned int &size,
        std::vector<uint8_t> &data);

    void addSegment(const Segment &seg);
    void crop(unsigned int minimum_address, unsigned int maximum_address);
    unsigned int getMaximumAdressOfLastSegment();
    void removeSegmentsBetween(unsigned int minimum_address, unsigned int maximum_address);


    std::vector<Chunk> chunked(std::string hexfile, BootAttrs bootattrs);

    std::vector<Chunk> chunks(unsigned int size, unsigned int alignment, std::vector<uint8_t> padding);

    void add_ihex(std::vector<std::string> records);

    unsigned int totalLength() const;
};

#endif /* HEXFILE_H */
