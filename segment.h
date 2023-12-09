#ifndef SEGMENT_H
#define SEGMENT_H

#include <vector>
#include <cstdint>
#include <stdexcept>



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

    bool remove_data(unsigned int new_min_address, unsigned int new_max_address, Segment &splitSegment);

    std::vector<Segment> chunks(unsigned int size, unsigned int alignment, std::vector<uint8_t> padding);

    unsigned int getSize();
};

#endif /* SEGMENT_H */
