#ifndef HEXFILE_H
#define HEXFILE_H

#include "macbootflash-c.cpp"

class Segment
{
public:
    unsigned int minimum_address;
    unsigned int maximum_address;
    std::vector<uint8_t> data;
    unsigned int word_size_bytes;

    Segment(unsigned int min_addr, unsigned int max_addr, std::vector<uint8_t> dat, unsigned int word_size);
    bool operator==(const Segment &other) const;
    unsigned int address() const;
    void add_data(unsigned int min_addr, unsigned int max_addr, const std::vector<uint8_t> &new_data);
};

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
    std::vector<Segment> segments;

    unsigned int word_size_bytes;

    
    unsigned int execution_start_address;

public:
    std::vector<Segment> debug_segments;
    
    HexFile();
    unsigned int crc_ihex(const std::vector<uint8_t> &bytes);

    void unpack_ihex(
        const std::string &record,
        unsigned int &type_,
        unsigned int &address,
        unsigned int &size,
        std::vector<uint8_t> &data);

    std::vector<Chunk> chunked(std::string hexfile, BootAttrs bootattrs);

    void add_ihex(std::vector<std::string> records);
};

#endif /* HEXFILE_H */
