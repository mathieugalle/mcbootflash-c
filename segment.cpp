
#include "segment.h"

#include <iostream>

#include <sstream>
#include <iomanip>

std::string bytesToHexString(const std::vector<uint8_t> &bytes)
{
    std::stringstream ss;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(bytes[i]);
        if (i < bytes.size() - 1)
            ss << " ";
    }
    return ss.str();
}

// Segment
Segment::Segment(unsigned int min_addr, unsigned int max_addr, std::vector<uint8_t> dat, unsigned int word_size)
    : minimum_address(min_addr), maximum_address(max_addr), data(dat), word_size_bytes(word_size)
{
}

bool Segment::operator==(const Segment &other) const
{
    return (minimum_address == other.minimum_address) &&
           (maximum_address == other.maximum_address) &&
           (data == other.data) &&
           (word_size_bytes == other.word_size_bytes);
}
unsigned int Segment::address() const
{
    return minimum_address / word_size_bytes;
}

void Segment::add_data(unsigned int min_addr, unsigned int max_addr, const std::vector<uint8_t> &new_data)
{
    if (min_addr == maximum_address)
    {
        maximum_address = max_addr;
        data.insert(data.end(), new_data.begin(), new_data.end());
    }
    else if (max_addr == minimum_address)
    {
        minimum_address = min_addr;
        data.insert(data.begin(), new_data.begin(), new_data.end());
    }
    else
    {
        throw std::runtime_error("data added to a segment must be adjacent to the original segment data");
    }
}

bool Segment::remove_data(unsigned int new_min_address, unsigned int new_max_address, Segment &splitSegment)
{
    // Vérification des plages d'adresses
    if ((new_min_address >= maximum_address) || (new_max_address <= minimum_address))
    {
        return false; // TODO : vérifier que la plage est exclue dans ce cas ?!
    }

    // Ajustement des adresses
    if (new_min_address < minimum_address)
        new_min_address = minimum_address;

    if (new_max_address > maximum_address)
        new_max_address = maximum_address;

    unsigned int remove_size = new_max_address - new_min_address;
    unsigned int part1_size = new_min_address - minimum_address;
    std::vector<uint8_t> part1_data(data.begin(), data.begin() + part1_size);
    std::vector<uint8_t> part2_data(data.begin() + part1_size + remove_size, data.end());

    bool isSplitted = false;

    if (!part1_data.empty() && !part2_data.empty())
    {
        // Mise à jour de ce segment et définition du segment divisé
        maximum_address = new_min_address + part1_size;
        data = part1_data;

        splitSegment = Segment(
            new_max_address,
            maximum_address + part2_data.size(),
            part2_data,
            word_size_bytes);
        isSplitted = true;
    }
    else
    {
        // Mise à jour de ce segment uniquement
        if (!part1_data.empty())
        {
            maximum_address = new_min_address;
            data = part1_data;
        }
        else if (!part2_data.empty())
        {
            minimum_address = new_max_address;
            data = part2_data;
        }
        else
        {
            maximum_address = minimum_address;
            data.clear();
        }
    }

    return isSplitted;
}

/// @brief generates data chunks of `size` words, aligned as given by `alignment`. Each chunk is itself a Segment.
// `size` and `alignment` are in words. `size` must be a multiple of
// `alignment`. If set, `padding` must be a word value.
// If `padding` is set, the first and final chunks are padded so that:
//     1. The first chunk is aligned even if the segment itself is not.
//     2. The final chunk's size is a multiple of `alignment`.
/// @param size
/// @param alignment
/// @param padding
/// @return
std::vector<Segment> Segment::chunks(unsigned int size, unsigned int alignment, std::vector<uint8_t> padding)
{
    if ((size % alignment) != 0)
    {
        throw std::invalid_argument("size is not a multiple of alignment");
    }
    // Si il y a un padding, il doit faire la taille d'un seul mot
    if (!padding.empty() && padding.size() != word_size_bytes)
    {
        throw std::invalid_argument("padding must be a word value");
    }

    std::vector<uint8_t> tmp = data;

    // std::cout << "zeroth tmp : " << std::endl;
    // std::cout << "size : " << tmp.size() << std::endl;
    // std::cout << bytesToHexString(tmp) << std::endl;

    unsigned int byte_size = size * word_size_bytes;
    unsigned int byte_alignment = alignment * word_size_bytes;
    unsigned int address = minimum_address;
    std::vector<Segment> results;

    // Apply padding to first and final chunk, if necessary
    unsigned int align_offset = address % byte_alignment;
    if (!padding.empty())
    {
        address -= align_offset;
        // on insère devant
        unsigned int num_padding = align_offset / word_size_bytes;
        while (num_padding > 0)
        {
            tmp.insert(tmp.begin(), padding.begin(), padding.end());
            --num_padding;
        }

        // std::cout << "tmp size after adding before: " << tmp.size() << std::endl;

        // on insère derrière
        /*en python :
alignment :  8
word_size_bytes :  2
len of data :  1016
number of paddings  0
*/
        // std::cout << "byte_alignment : " << byte_alignment << std::endl;
        // std::cout << "word_size_bytes : " << word_size_bytes << std::endl;
        // std::cout << "tmp.size() : " << tmp.size() << std::endl;
        // unsigned int padding_to_add = ( ((int)byte_alignment - (int)tmp.size()) % byte_alignment) / word_size_bytes;
        unsigned int padding_to_add = ((byte_alignment - tmp.size()) % byte_alignment) / word_size_bytes;
        // std::cout << "padding_to_add " << padding_to_add << std::endl;
        // std::cout << "modulo test : " << (-1008 % 8) << std::endl;
        // std::cout << "substract test " << (byte_alignment - tmp.size()) << std::endl;
        for (unsigned int i = 0; i < padding_to_add; ++i)
        {
            tmp.insert(tmp.end(), padding.begin(), padding.end());
        }
    }

    // std::cout << "first tmp : " << std::endl;
    // std::cout << "size : " << tmp.size() << std::endl;
    // std::cout << bytesToHexString(tmp) << std::endl;

    // First chunk may be non-aligned and shorter than `byte_size` if padding is empty
    unsigned int chunk_offset = address % byte_alignment;
    if (chunk_offset != 0)
    {
        unsigned int first_chunk_size = byte_alignment - chunk_offset;
        results.push_back(Segment(address,
                                  address + first_chunk_size,
                                  std::vector<uint8_t>(tmp.begin(), tmp.begin() + first_chunk_size),
                                  word_size_bytes));
        address += first_chunk_size;
        tmp.erase(tmp.begin(), tmp.begin() + first_chunk_size); // Mise à jour de 'tmp'
    }

    // Process remaining data in tmp
    for (size_t offset = 0; offset < tmp.size(); offset += byte_size)
    {
        int current_chunk_size = std::min((int)byte_size, (int)tmp.size() - (int)offset);

        // if(offset == 720 or offset == 960) {
        //     std::cout << "offset : " << offset << std::endl;
        //     std::cout << "address + offset " << address + offset << std::endl;
        //     std::cout << "address + offset + byte_size " << address + offset + byte_size << std::endl;
        //     std::cout << "data[offset:offset + size] len " << std::vector<uint8_t>(tmp.begin() + offset, tmp.begin() + offset + current_chunk_size).size() << std::endl;
        // }
        results.push_back(
            Segment(address + offset,
                    address + offset + byte_size,
                    std::vector<uint8_t>(tmp.begin() + offset, tmp.begin() + offset + current_chunk_size),
                    word_size_bytes));
    }

    return results;
}

//        def __len__(self) in python
unsigned int Segment::getSize()
{
    return data.size() / word_size_bytes;
}