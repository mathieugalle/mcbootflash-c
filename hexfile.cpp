#include "doctest.h"

#include <vector>
#include <string>
#include <fstream>

#include <stdexcept>
#include "hexfile.h"
#include <algorithm> //std::remove

std::vector<uint8_t> hexStringToBytes(const std::string &str)
{
    std::vector<uint8_t> bytes;
    std::string hexStr = str;
    hexStr.erase(std::remove(hexStr.begin(), hexStr.end(), ' '), hexStr.end());

    for (size_t i = 0; i < hexStr.length(); i += 2)
    {
        std::string byteString = hexStr.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

HexFile::HexFile() : current_segment_index(-1), word_size_bytes(0), execution_start_address(0), processed_total_bytes(0)
{
    // default hexfile constructor
}

unsigned int HexFile::crc_ihex(const std::vector<uint8_t> &bytes)
{
    unsigned int crc = 0;
    for (auto byte : bytes)
    {
        crc += byte;
    }
    crc &= 0xff;
    crc = (~crc + 1) & 0xff;
    return crc;
}

void HexFile::unpack_ihex(const std::string &record, unsigned int &type_, unsigned int &address, unsigned int &size, std::vector<uint8_t> &data)
{
    if (record.length() < 11 || record[0] != ':')
    {
        throw std::runtime_error("Invalid record format");
    }

    std::vector<uint8_t> bytes;
    for (size_t i = 1; i < record.length(); i += 2)
    {
        std::string byteString = record.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoi(byteString, nullptr, 16)));
    }

    size = bytes[0];
    if (size != bytes.size() - 5)
    {
        throw std::runtime_error("Incorrect record size");
    }

    address = (bytes[1] << 8) | bytes[2];
    type_ = bytes[3];
    data = std::vector<uint8_t>(bytes.begin() + 4, bytes.end() - 1);

    unsigned int actual_crc = bytes.back();

    // Exclut le dernier élément (CRC)
    std::vector<uint8_t> crcBytes(bytes.begin(), bytes.end() - 1);
    unsigned int expected_crc = crc_ihex(crcBytes);

    if (actual_crc != expected_crc)
    {
        std::stringstream ss;
        ss << "CRC mismatch: expected " << std::hex << std::setw(2) << std::setfill('0') << expected_crc
           << ", but got " << actual_crc;
        throw std::runtime_error(ss.str());
    }
}

/**
 * Add given Intel HEX records string.
 */
void HexFile::add_ihex(std::vector<std::string> lines)
{
    unsigned int extended_segment_address = 0;
    unsigned int extended_linear_address = 0;

    for (unsigned int i = 0; i < lines.size(); i++)
    {
        std::string line = lines[i];

        unsigned int lineType = 0;
        unsigned int lineAddress = 0;
        unsigned int lineSize = 0;
        std::vector<uint8_t> lineData;
        unpack_ihex(line, lineType, lineAddress, lineSize, lineData);

        if (lineType == IHEX_DATA) // Data record
        {
            debug_segments.push_back(Segment(
                lineAddress,
                lineAddress + lineSize,
                lineData,
                word_size_bytes));

            addSegment(Segment(
                lineAddress,
                lineAddress + lineSize,
                lineData,
                word_size_bytes));
        }
        else if (lineType == IHEX_END_OF_FILE)
        {
            // Pas de traitement spécial requis pour la fin de fichier
            // std::cout << "IHEX_END_OF_FILE" << std::endl;
        }
        else if (lineType == IHEX_EXTENDED_SEGMENT_ADDRESS)
        {
            extended_segment_address = static_cast<unsigned int>((lineData[0] << 8) | lineData[1]);
            extended_segment_address *= 16;
        }
        else if (lineType == IHEX_EXTENDED_LINEAR_ADDRESS)
        {
            extended_linear_address = static_cast<unsigned int>((lineData[0] << 8) | lineData[1]);
            extended_linear_address <<= 16;
        }
        else if (lineType == IHEX_START_SEGMENT_ADDRESS || lineType == IHEX_START_LINEAR_ADDRESS)
        {
            execution_start_address = static_cast<unsigned int>((lineData[0] << 8) | lineData[1]);
        }
        else
        {
            throw std::runtime_error("Unexpected record type");
        }
    }
}

void HexFile::addSegment(const Segment &newSeg)
{
    if (segments.empty())
    {
        segments.push_back(newSeg);
        current_segment_index = 0;
        return;
    }
    if (current_segment_index == -1)
    {
        throw std::runtime_error("current segment index should be different of -1 if segments in not empty ?!");
    }

    // Quick insertion for adjacent segment with the previously inserted segment
    if (newSeg.minimum_address == segments[current_segment_index].maximum_address)
    {
        segments[current_segment_index].add_data(newSeg.minimum_address,
                                                 newSeg.maximum_address,
                                                 newSeg.data);
        return;
    }

    // linear insert
    unsigned int i = 0;
    for (; i < segments.size(); ++i)
    {
        if (newSeg.minimum_address <= segments[i].maximum_address)
        {
            break;
        }
    }

    if (i == segments.size())
    {
        // Si on est arrivé ici, cela signifie que le segment est après tous les autres
        // parce que le boucle n'est jamais arrivée sur le "break"
        segments.push_back(newSeg);
        current_segment_index = segments.size() - 1;
    }
    else if (i > segments.size())
    {
        throw std::runtime_error("should be impossible : i > segments.size()");
    }
    else // i < segments.size()
    {
        if (newSeg.maximum_address < segments[i].minimum_address)
        {
            // Non-overlapping, non-adjacent before
            segments.insert(segments.begin() + i, newSeg);
            current_segment_index = i;
        }
        else
        {
            // Adjacent or overlapping
            segments[i].add_data(newSeg.minimum_address,
                                 newSeg.maximum_address,
                                 newSeg.data);
            current_segment_index = i;
        }
    }

    Segment &current_segment = segments[current_segment_index];
    while (current_segment_index < (int)(segments.size()) - 1)
    {
        Segment &next_segment = segments[current_segment_index + 1];

        if (current_segment.maximum_address >= next_segment.maximum_address)
        {
            // Le segment suivant est complètement recouvert
            segments.erase(segments.begin() + current_segment_index + 1);
        }
        else if (current_segment.maximum_address >= next_segment.minimum_address)
        {
            // Les segments sont adjacents ou se chevauchent partiellement
            // On n'ajoute que la partie nécessaire de next_segment.data
            unsigned int start = current_segment.maximum_address - next_segment.minimum_address;
            std::vector<uint8_t> partialData(next_segment.data.begin() + start, next_segment.data.end());
            current_segment.add_data(current_segment.maximum_address,
                                     next_segment.maximum_address,
                                     partialData);
            segments.erase(segments.begin() + current_segment_index + 1);
            break;
        }
        else
        {
            // Les segments ne se chevauchent pas, ni ne sont adjacents
            break;
        }
    }
}

unsigned int HexFile::getMaximumAdressOfLastSegment()
{
    if (segments.size() == 0)
    {
        throw std::runtime_error("no segments to get maximum adress from");
    }
    else
    {
        return segments[segments.size() - 1].maximum_address;
    }
}

unsigned int HexFile::totalLength() const
{
    unsigned int length = 0;
    for (const Segment &segment : segments)
    {
        length += segment.data.size();
    }
    length /= word_size_bytes; // Divise par la taille du mot
    return length;
}

/// @brief Keep given range and discard the rest.
/// @param minimum_address is the first word address to keep (including)
/// @param maximum_address is the last word address to keep (excluding).
void HexFile::crop(unsigned int minimum_address, unsigned int maximum_address)
{
    // Ajuster les adresses en fonction de word_size_bytes
    minimum_address *= word_size_bytes;
    maximum_address *= word_size_bytes;
    unsigned int current_address_address = getMaximumAdressOfLastSegment();

    removeSegmentsBetween(0, minimum_address);
    removeSegmentsBetween(maximum_address, current_address_address);
}

// same as     def remove(self, minimum_address, maximum_address):
void HexFile::removeSegmentsBetween(unsigned int minimum_address, unsigned int maximum_address)
{
    // Nouvelle liste pour stocker les segments modifiés
    std::vector<Segment> new_segments;
    for (unsigned int i = 0; i < segments.size(); i++)
    {
        Segment &segment = segments[i];
        // Appliquer remove_data sur chaque segment

        Segment split(0, 0, {}, 0);
        bool isSplitted = segment.remove_data(minimum_address, maximum_address, split);

        // Ajouter le segment s'il a toujours des données valides
        if (segment.minimum_address < segment.maximum_address)
        {
            new_segments.push_back(segment);
        }

        // Ajouter le segment split s'il existe et est différent du segment actuel
        if (isSplitted)
        {
            new_segments.push_back(split);
        }
    }
    // Mettre à jour la liste des segments
    segments = new_segments;
}

// Vire les espaces au début et à la fin
std::string strip(const std::string &str)
{
    size_t start = 0;
    size_t end = str.size();
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (!isspace(str[i]))
        {
            start = i;
            break;
        }
    }
    for (size_t i = str.size(); i > 0; --i)
    {
        if (!isspace(str[i - 1]))
        {
            end = i;
            break;
        }
    }
    return str.substr(start, end - start);
}

std::vector<Segment> HexFile::chunked(std::string hexfile, BootAttrs bootattrs)
{
    std::ifstream file(hexfile);
    std::string line;

    // unsigned int execution_start_address = 0;

    if (file.is_open())
    {
        // std::cout << "file is open" << std::endl;
    }
    else
    {
        std::cout << "file is NOT open" << std::endl;
        return std::vector<Segment>();
    }

    word_size_bytes = 1;

    std::vector<std::string> lines;
    while (std::getline(file, line))
    {
        line = strip(line);
        if (line.empty() || line[0] != ':')
        {
            throw std::runtime_error("Invalid format");
        }
        lines.push_back(line);
    }

    add_ihex(lines);

    // std::cout << "at this point before crop, I have " << segments.size() << " segments" << std::endl;
    for (unsigned int i = 0; i < segments.size(); i++)
    {
        debug_segments_before_crop.push_back(Segment(
            segments[i].minimum_address,
            segments[i].maximum_address,
            segments[i].data,
            segments[i].word_size_bytes + 1));
    }

    crop(bootattrs.memory_start, bootattrs.memory_end);
    // std::cout << "at this point after crop, I have " << debug_segments_before_crop.size() << " segments in debug_segments_before_crop" << std::endl;

    word_size_bytes = 2;
    for (unsigned int i = 0; i < segments.size(); i++)
    {
        segments[i].word_size_bytes = 2;
    }

    unsigned int chunk_size = bootattrs.max_packet_length - Command::getSize();
    chunk_size -= chunk_size % bootattrs.write_size;
    chunk_size /= word_size_bytes; // division entière
    unsigned int total_bytes = totalLength() * word_size_bytes;

    if (total_bytes == 0)
    {
        throw std::runtime_error("HEX file contains no data within program memory range");
    }

    total_bytes += (bootattrs.write_size - total_bytes) % bootattrs.write_size;
    unsigned int align = bootattrs.write_size / word_size_bytes; // division entière encore

    processed_total_bytes = total_bytes;
    std::vector<uint8_t> twoBytes{0, 0};

    std::vector<Segment> res;

    // std::cout << "chunk_size : " << chunk_size << std::endl;
    // std::cout << "align : " << align << std::endl;
    res = chunks(chunk_size, align, twoBytes);
    return res;
}



std::vector<Segment> HexFile::chunks(unsigned int size, unsigned int alignment, std::vector<uint8_t> padding)
{
    if (size % alignment != 0)
    {
        throw std::invalid_argument("size is not a multiple of alignment");
    }

    if (!padding.empty() && padding.size() != word_size_bytes)
    {
        throw std::invalid_argument("padding must be a word value");
    }

    std::vector<Segment> result;
    Segment previous(0, 0, {}, word_size_bytes);

    // en supposant que HexFile peut être itéré pour obtenir des Segment
    for (unsigned int i = 0; i < segments.size(); i++)
    {
        Segment &segment = segments[i];
        std::vector<Segment> segment_chunks = segment.chunks(size, alignment, padding);

        for (unsigned int j = 0; j < segment_chunks.size(); j++)
        {
            Segment &chunk = segment_chunks[j];
            if (chunk.address() < previous.address() + previous.getSize())
            {
                // Fusionner les chunks chevauchants
                std::vector<uint8_t> merged;
                std::vector<uint8_t> low = std::vector<uint8_t>(
                    previous.data.end() - alignment * word_size_bytes,
                    previous.data.end());

                std::vector<uint8_t> high = std::vector<uint8_t>(
                    chunk.data.begin(),
                    chunk.data.begin() + alignment * word_size_bytes);

                merged.reserve(alignment * word_size_bytes);

                // ???
                for (size_t i = 0; i < alignment * word_size_bytes; ++i)
                {
                    // XOR des octets et ajout du padding si nécessaire
                    uint8_t merged_byte = low[i] ^ high[i] ^ (padding.empty() ? 0 : padding[i % padding.size()]);
                    merged.push_back(merged_byte);
                }

                chunk.data = std::vector<uint8_t>(merged.begin(), merged.end());
                chunk.data.insert(chunk.data.end(), chunk.data.begin() + alignment * word_size_bytes, chunk.data.end());
            }
            result.push_back(chunk);
        }
        // ???
        previous = segment_chunks.back();
    }

    return result;
}
