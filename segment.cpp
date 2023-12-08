
#include "segment.h"


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


std::vector<Chunk> Segment::chunks(unsigned int size, unsigned int alignment, std::vector<uint8_t> padding) {

    return std::vector<Chunk>();
}
