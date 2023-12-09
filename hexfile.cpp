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

TEST_CASE("crc_ihex check 1")
{
    HexFile hex;
    // in python : "010203aaff"
    std::vector<uint8_t> bytes{0x01, 0x02, 0x03, 0xAA, 0xFF};

    unsigned int crc = hex.crc_ihex(bytes);
    CHECK(crc == 81);
}
TEST_CASE("crc_ihex check 2")
{
    HexFile hex;
    std::vector<uint8_t> bytes{67, 56, 89, 45};
    unsigned int crc = hex.crc_ihex(bytes);
    CHECK(crc == 255);
}
TEST_CASE("crc_ihex check 3")
{
    HexFile hex;
    // "588405bc7be1f154bbab51427e254050425dcf55"
    std::vector<uint8_t> bytes{0x58, 0x84, 0x05, 0xbc, 0x7b, 0xe1, 0xf1, 0x54, 0xbb, 0xab, 0x51, 0x42, 0x7e, 0x25, 0x40, 0x50, 0x42, 0x5d, 0xcf, 0x55};
    unsigned int crc = hex.crc_ihex(bytes);
    CHECK(crc == 211);
}
TEST_CASE("crc_ihex check 4")
{
    std::string hexAsStr("10000000E01A040000000000041A0000081A0000");
    std::vector<uint8_t> bytes = hexStringToBytes(hexAsStr);
    HexFile hex;
    unsigned int crc = hex.crc_ihex(bytes);
    CHECK(crc == 178);
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
TEST_CASE("unpack_ihex check if IHEX_DATA")
{
    HexFile hex;
    std::string record(":10000000E01A040000000000041A0000081A0000B2");
    unsigned int type = 54;
    unsigned int address = 0;
    unsigned int size = 0;
    std::vector<uint8_t> data;

    hex.unpack_ihex(record, type, address, size, data);

    CHECK(type == IHEX_DATA);
    CHECK(address == 0);
    CHECK(size == 16);
    CHECK(bytesToHexString(data) == "e0 1a 04 00 00 00 00 00 04 1a 00 00 08 1a 00 00");
}

TEST_CASE("unpack_ihex check if IHEX_END_OF_FILE")
{
    HexFile hex;
    std::string record(":00000001FF");
    unsigned int type = 54;
    unsigned int address = 0;
    unsigned int size = 0;
    std::vector<uint8_t> data;

    hex.unpack_ihex(record, type, address, size, data);

    CHECK(type == IHEX_END_OF_FILE);
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

#define FLASH_HEX_FILE "/home/yoctouser/mcbootflash-c/mcbootflash/tests/testcases/flash/test.hex"
BootAttrs defaultBootAttrsForTest()
{
    BootAttrs bootattrs;
    bootattrs.version = 258,
    bootattrs.max_packet_length = 256,
    bootattrs.device_id = 13398,
    bootattrs.erase_size = 2048,
    bootattrs.write_size = 8,
    bootattrs.memory_start = 6144;
    bootattrs.memory_end = 174080;
    bootattrs.has_checksum = true;
    return bootattrs;
}

std::vector<Segment> debugSegmentsFromPython()
{
    return std::vector<Segment>{
        Segment(0, 16, hexStringToBytes("e01a040000000000041a0000081a0000"), 1),
        Segment(16, 32, hexStringToBytes("0c1a0000101a0000141a0000181a0000"), 1),
        Segment(32, 48, hexStringToBytes("1c1a0000001a0000201a0000241a0000"), 1),
        Segment(48, 64, hexStringToBytes("281a00002c1a0000301a0000341a0000"), 1),
        Segment(64, 80, hexStringToBytes("381a00003c1a0000401a0000441a0000"), 1),
        Segment(80, 96, hexStringToBytes("481a00004c1a0000501a0000541a0000"), 1),
        Segment(96, 112, hexStringToBytes("581a0000001a00005c1a0000601a0000"), 1),
        Segment(112, 128, hexStringToBytes("641a0000681a00006c1a0000001a0000"), 1),
        Segment(128, 144, hexStringToBytes("001a0000001a0000701a0000741a0000"), 1),
        Segment(144, 160, hexStringToBytes("781a00007c1a0000801a0000841a0000"), 1),
        Segment(160, 176, hexStringToBytes("881a00008c1a0000901a0000941a0000"), 1),
        Segment(176, 192, hexStringToBytes("001a0000001a0000981a00009c1a0000"), 1),
        Segment(192, 208, hexStringToBytes("a01a0000001a0000001a0000001a0000"), 1),
        Segment(208, 224, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(224, 240, hexStringToBytes("001a0000001a0000001a0000a41a0000"), 1),
        Segment(240, 256, hexStringToBytes("a81a0000001a0000001a0000001a0000"), 1),
        Segment(256, 272, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(272, 288, hexStringToBytes("ac1a0000001a0000001a0000001a0000"), 1),
        Segment(288, 304, hexStringToBytes("001a0000001a0000001a0000b01a0000"), 1),
        Segment(304, 320, hexStringToBytes("b41a0000b81a0000001a0000001a0000"), 1),
        Segment(320, 336, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(336, 352, hexStringToBytes("001a0000001a0000001a0000bc1a0000"), 1),
        Segment(352, 368, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(368, 384, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(384, 400, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(400, 416, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(416, 432, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(432, 448, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(448, 464, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(464, 480, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(480, 496, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(496, 512, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(512, 528, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(528, 544, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(544, 560, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(560, 576, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(576, 592, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(592, 608, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(608, 624, hexStringToBytes("c01a0000c41a0000001a0000c81a0000"), 1),
        Segment(624, 640, hexStringToBytes("cc1a0000d01a0000d41a0000d81a0000"), 1),
        Segment(640, 656, hexStringToBytes("dc1a0000001a0000001a0000001a0000"), 1),
        Segment(656, 672, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(672, 688, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(688, 704, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(704, 720, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(720, 736, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(736, 752, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(752, 768, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(768, 784, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(784, 800, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(800, 816, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(816, 832, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(832, 848, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(848, 864, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(864, 880, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(880, 896, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(896, 912, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(912, 928, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(928, 944, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(944, 960, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(960, 976, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(976, 992, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(992, 1008, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(1008, 1024, hexStringToBytes("001a0000001a0000001a0000001a0000"), 1),
        Segment(12288, 12304, hexStringToBytes("e01a0400000000000200fa00000f7800"), 1),
        Segment(12304, 12320, hexStringToBytes("1e00780000407800674060000080fb00"), 1),
        Segment(12320, 12336, hexStringToBytes("670060004a00dd00020a8000f13f2e00"), 1),
        Segment(12336, 12352, hexStringToBytes("8100610001007000000a88000080fa00"), 1),
        Segment(12352, 12368, hexStringToBytes("000006000200fa00000f78001e007800"), 1),
        Segment(12368, 12384, hexStringToBytes("00407800674060000080fb0067006000"), 1),
        Segment(12384, 12400, hexStringToBytes("020a800081ff2f008100610001007000"), 1),
        Segment(12400, 12416, hexStringToBytes("000a88000080fa00000006000000fa00"), 1),
        Segment(12416, 12432, hexStringToBytes("4301a8000080fa00000006000000fa00"), 1),
        Segment(12432, 12448, hexStringToBytes("0028a9000080fa00000006000200fa00"), 1),
        Segment(12448, 12464, hexStringToBytes("000f78001e0078000040780067406000"), 1),
        Segment(12464, 12480, hexStringToBytes("0080fb00670060004a00dd00420a8000"), 1),
        Segment(12480, 12496, hexStringToBytes("f13f2e008100610001007000400a8800"), 1),
        Segment(12496, 12512, hexStringToBytes("0080fa00000006000200fa00000f7800"), 1),
        Segment(12512, 12528, hexStringToBytes("1e00780000407800674060000080fb00"), 1),
        Segment(12528, 12544, hexStringToBytes("67006000420a800081ff2f0081006100"), 1),
        Segment(12544, 12560, hexStringToBytes("01007000400a88000080fa0000000600"), 1),
        Segment(12560, 12576, hexStringToBytes("0000fa004b01a8000080fa0000000600"), 1),
        Segment(12576, 12592, hexStringToBytes("0200fa00000f78001e00780000407800"), 1),
        Segment(12592, 12608, hexStringToBytes("674060000080fb00670060004a00dd00"), 1),
        Segment(12608, 12624, hexStringToBytes("820a8000f13f2e008100610001007000"), 1),
        Segment(12624, 12640, hexStringToBytes("800a88000080fa00000006000200fa00"), 1),
        Segment(12640, 12656, hexStringToBytes("000f78001e0078000040780067406000"), 1),
        Segment(12656, 12672, hexStringToBytes("0080fb0067006000820a800081ff2f00"), 1),
        Segment(12672, 12688, hexStringToBytes("8100610001007000800a88000080fa00"), 1),
        Segment(12688, 12704, hexStringToBytes("000006000000fa005301a8000080fa00"), 1),
        Segment(12704, 12720, hexStringToBytes("000006000000fa0004a8a9000080fa00"), 1),
        Segment(12720, 12736, hexStringToBytes("000006000200fa00000f78001e007800"), 1),
        Segment(12736, 12752, hexStringToBytes("00407800674060000080fb0067006000"), 1),
        Segment(12752, 12768, hexStringToBytes("4a00dd00c20a8000f13f2e0081006100"), 1),
        Segment(12768, 12784, hexStringToBytes("01007000c00a88000080fa0000000600"), 1),
        Segment(12784, 12800, hexStringToBytes("0200fa00000f78001e00780000407800"), 1),
        Segment(12800, 12816, hexStringToBytes("674060000080fb0067006000c20a8000"), 1),
        Segment(12816, 12832, hexStringToBytes("81ff2f008100610001007000c00a8800"), 1),
        Segment(12832, 12848, hexStringToBytes("0080fa00000006000000fa005b01a800"), 1),
        Segment(12848, 12864, hexStringToBytes("0080fa00000006000600fa00004f7800"), 1),
        Segment(12864, 12880, hexStringToBytes("1147980012079800230798001e80fb00"), 1),
        Segment(12880, 12896, hexStringToBytes("a1b9260000804000104078000074a100"), 1),
        Segment(12896, 12912, hexStringToBytes("8080fb00f00720000080600072358000"), 1),
        Segment(12912, 12928, hexStringToBytes("01f82f00810061000100700070358800"), 1),
        Segment(12928, 12944, hexStringToBytes("1e4090000080fb00a1b9260000804000"), 1),
        Segment(12944, 12960, hexStringToBytes("104078000074a1008080fb00f0072000"), 1),
        Segment(12960, 12976, hexStringToBytes("008060008235800001f82f0081006100"), 1),
        Segment(12976, 12992, hexStringToBytes("01007000803588003048070093480700"), 1),
        Segment(12992, 13008, hexStringToBytes("f648070060470700700020004eff0700"), 1),
        Segment(13008, 13024, hexStringToBytes("7000200071ff07007000200090ff0700"), 1),
        Segment(13024, 13040, hexStringToBytes("70002000b3ff070064ff070088ff0700"), 1),
        Segment(13040, 13056, hexStringToBytes("a8ff0700ccff070064ff0700a9ff0700"), 1),
        Segment(13056, 13072, hexStringToBytes("1e0090004fff07001e00900072ff0700"), 1),
        Segment(13072, 13088, hexStringToBytes("2e00900091ff07002e009000b4ff0700"), 1),
        Segment(13088, 13104, hexStringToBytes("50480700b34807001649070080470700"), 1),
        Segment(13104, 13120, hexStringToBytes("0080fa00000006000200fa00fb420700"), 1),
        Segment(13120, 13136, hexStringToBytes("004f7800054d07001021a8001e407800"), 1),
        Segment(13136, 13152, hexStringToBytes("e44f500002003a00b24b070016410700"), 1),
        Segment(13152, 13168, hexStringToBytes("1e80fb00a1b926000080400010407800"), 1),
        Segment(13168, 13184, hexStringToBytes("0074a1008080fb00f007200000806000"), 1),
        Segment(13184, 13200, hexStringToBytes("3235800001f82f008100610001007000"), 1),
        Segment(13200, 13216, hexStringToBytes("303588000b4d070010c0b3000080fa00"), 1),
        Segment(13216, 13232, hexStringToBytes("000006000000fa00024d070006430700"), 1),
        Segment(13232, 13248, hexStringToBytes("10c0b3000080fa0000000600f03fb100"), 1),
        Segment(13248, 13264, hexStringToBytes("0180b10006003500ee03090000000000"), 1),
        Segment(13264, 13280, hexStringToBytes("403fb1000180b100fbff3d001000b000"), 1),
        Segment(13280, 13296, hexStringToBytes("203fb000020035000080090000000000"), 1),
        Segment(13296, 13304, hexStringToBytes("00000600ffff3700"), 1)};
}

// only the second one survive
std::vector<Segment> debugSegmentsBeforeCropFromPython()
{
    return std::vector<Segment>{
        Segment(0, 1024, hexStringToBytes("e01a040000000000041a0000081a00000c1a0000101a0000141a0000181a00001c1a0000001a0000201a0000241a0000281a00002c1a0000301a0000341a0000381a00003c1a0000401a0000441a0000481a00004c1a0000501a0000541a0000581a0000001a00005c1a0000601a0000641a0000681a00006c1a0000001a0000001a0000001a0000701a0000741a0000781a00007c1a0000801a0000841a0000881a00008c1a0000901a0000941a0000001a0000001a0000981a00009c1a0000a01a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000a41a0000a81a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000ac1a0000001a0000001a0000001a0000001a0000001a0000001a0000b01a0000b41a0000b81a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000bc1a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000c01a0000c41a0000001a0000c81a0000cc1a0000d01a0000d41a0000d81a0000dc1a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000001a0000"), 2),
        Segment(12288, 13304, hexStringToBytes("e01a0400000000000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00020a8000f13f2e008100610001007000000a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000020a800081ff2f008100610001007000000a88000080fa00000006000000fa004301a8000080fa00000006000000fa000028a9000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00420a8000f13f2e008100610001007000400a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000420a800081ff2f008100610001007000400a88000080fa00000006000000fa004b01a8000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00820a8000f13f2e008100610001007000800a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000820a800081ff2f008100610001007000800a88000080fa00000006000000fa005301a8000080fa00000006000000fa0004a8a9000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00c20a8000f13f2e008100610001007000c00a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000c20a800081ff2f008100610001007000c00a88000080fa00000006000000fa005b01a8000080fa00000006000600fa00004f78001147980012079800230798001e80fb00a1b9260000804000104078000074a1008080fb00f0072000008060007235800001f82f008100610001007000703588001e4090000080fb00a1b9260000804000104078000074a1008080fb00f0072000008060008235800001f82f008100610001007000803588003048070093480700f648070060470700700020004eff07007000200071ff07007000200090ff070070002000b3ff070064ff070088ff0700a8ff0700ccff070064ff0700a9ff07001e0090004fff07001e00900072ff07002e00900091ff07002e009000b4ff070050480700b348070016490700804707000080fa00000006000200fa00fb420700004f7800054d07001021a8001e407800e44f500002003a00b24b0700164107001e80fb00a1b9260000804000104078000074a1008080fb00f0072000008060003235800001f82f008100610001007000303588000b4d070010c0b3000080fa00000006000000fa00024d07000643070010c0b3000080fa0000000600f03fb1000180b10006003500ee03090000000000403fb1000180b100fbff3d001000b000203fb00002003500008009000000000000000600ffff3700"), 2),
    };
}

TEST_CASE("chunked function debug_segments generation")
{
    BootAttrs bootattrs = defaultBootAttrsForTest();

    HexFile hex;
    hex.chunked(FLASH_HEX_FILE, bootattrs);

    CHECK(hex.debug_segments.size() == 128);

    std::vector<Segment> debugSegments = debugSegmentsFromPython();

    for (unsigned int i = 0; i < debugSegments.size(); i++)
    {
        CHECK(hex.debug_segments[i] == debugSegments[i]);
    }
}

TEST_CASE("chunked function addSegments : debug_segment_before_crop")
{
    BootAttrs bootattrs = defaultBootAttrsForTest();

    HexFile hex;
    hex.chunked(FLASH_HEX_FILE, bootattrs);

    std::vector<Segment> debugSegmentsBeforeCrop = debugSegmentsBeforeCropFromPython();

    CHECK(hex.debug_segments_before_crop.size() == debugSegmentsBeforeCrop.size());

    for (unsigned int i = 0; i < debugSegmentsBeforeCrop.size(); i++)
    {
        CHECK(hex.debug_segments_before_crop[i].minimum_address == debugSegmentsBeforeCrop[i].minimum_address);
        CHECK(hex.debug_segments_before_crop[i].maximum_address == debugSegmentsBeforeCrop[i].maximum_address);
        CHECK(hex.debug_segments_before_crop[i].data == debugSegmentsBeforeCrop[i].data);
        CHECK(hex.debug_segments_before_crop[i].word_size_bytes == debugSegmentsBeforeCrop[i].word_size_bytes);
    }
}

TEST_CASE("chunked function crop : only one segment must survive, the second one")
{
    BootAttrs bootattrs = defaultBootAttrsForTest();

    HexFile hex;
    hex.chunked(FLASH_HEX_FILE, bootattrs);

    std::vector<Segment> debugSegmentsBeforeCropInPython = debugSegmentsBeforeCropFromPython();
    Segment cropSurvivorInPython = debugSegmentsBeforeCropInPython[1];
    CHECK(hex.segments.size() == 1);
    Segment cropSurvivorInSegments = hex.segments[0];

    CHECK(cropSurvivorInPython.minimum_address == cropSurvivorInPython.minimum_address);
    CHECK(cropSurvivorInSegments.maximum_address == cropSurvivorInPython.maximum_address);
    CHECK(cropSurvivorInSegments.data == cropSurvivorInPython.data);
    CHECK(cropSurvivorInSegments.word_size_bytes == cropSurvivorInPython.word_size_bytes);
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

std::vector<Segment> chunksSegmentsResultFromPython()
{
    return std::vector<Segment>{
        Segment(12288, 12528, hexStringToBytes("e01a0400000000000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00020a8000f13f2e008100610001007000000a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000020a800081ff2f008100610001007000000a88000080fa00000006000000fa004301a8000080fa00000006000000fa000028a9000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00420a8000f13f2e008100610001007000400a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb00"), 2),
        Segment(12528, 12768, hexStringToBytes("67006000420a800081ff2f008100610001007000400a88000080fa00000006000000fa004b01a8000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00820a8000f13f2e008100610001007000800a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000820a800081ff2f008100610001007000800a88000080fa00000006000000fa005301a8000080fa00000006000000fa0004a8a9000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00c20a8000f13f2e0081006100"), 2),
        Segment(12768, 13008, hexStringToBytes("01007000c00a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000c20a800081ff2f008100610001007000c00a88000080fa00000006000000fa005b01a8000080fa00000006000600fa00004f78001147980012079800230798001e80fb00a1b9260000804000104078000074a1008080fb00f0072000008060007235800001f82f008100610001007000703588001e4090000080fb00a1b9260000804000104078000074a1008080fb00f0072000008060008235800001f82f008100610001007000803588003048070093480700f648070060470700700020004eff0700"), 2),
        Segment(13008, 13248, hexStringToBytes("7000200071ff07007000200090ff070070002000b3ff070064ff070088ff0700a8ff0700ccff070064ff0700a9ff07001e0090004fff07001e00900072ff07002e00900091ff07002e009000b4ff070050480700b348070016490700804707000080fa00000006000200fa00fb420700004f7800054d07001021a8001e407800e44f500002003a00b24b0700164107001e80fb00a1b9260000804000104078000074a1008080fb00f0072000008060003235800001f82f008100610001007000303588000b4d070010c0b3000080fa00000006000000fa00024d07000643070010c0b3000080fa0000000600f03fb100"), 2),
        Segment(13248, 13488, hexStringToBytes("0180b10006003500ee03090000000000403fb1000180b100fbff3d001000b000203fb00002003500008009000000000000000600ffff3700"), 2),
    };
}

TEST_CASE("chunked function all segments")
{
    BootAttrs bootattrs = defaultBootAttrsForTest();
    HexFile hex;

    std::vector<Segment> chunks = hex.chunked(FLASH_HEX_FILE, bootattrs);

    std::vector<Segment> fromPython = chunksSegmentsResultFromPython();

    CHECK(chunks.size() == fromPython.size());
    unsigned int i = 0;
    for (i = 0; i < chunks.size() ; i++)
    {
        // std::cout << "chunk number " << i << std::endl;
        CHECK(chunks[i].word_size_bytes == fromPython[i].word_size_bytes);
        CHECK(chunks[i].minimum_address == fromPython[i].minimum_address);
        CHECK(chunks[i].maximum_address == fromPython[i].maximum_address);
        CHECK(chunks[i].data == fromPython[i].data);
    }
}

TEST_CASE("chunked function last segment")
{
    BootAttrs bootattrs = defaultBootAttrsForTest();
    HexFile hex;

    std::vector<Segment> chunks = hex.chunked(FLASH_HEX_FILE, bootattrs);

    std::vector<Segment> fromPython = chunksSegmentsResultFromPython();

    unsigned int i = chunks.size() - 1;
    CHECK(chunks[i].word_size_bytes == fromPython[i].word_size_bytes);
    CHECK(chunks[i].minimum_address == fromPython[i].minimum_address);
    CHECK(chunks[i].maximum_address == fromPython[i].maximum_address);
    CHECK(chunks[i].data == fromPython[i].data);
}

TEST_CASE("Segment.chunked function for last segment")
{
    BootAttrs bootattrs = defaultBootAttrsForTest();
    HexFile hex;
    hex.chunked(FLASH_HEX_FILE, bootattrs);
    std::vector<Segment> segments = hex.segments;
    Segment last = hex.segments[hex.segments.size() - 1];

    // from C++, not sure about this, but first 4 segments were right, so I guess it's good enough
    unsigned int chunk_size = 120;
    unsigned int align = 4;

    std::vector<uint8_t> twoBytes{0, 0};

    std::vector<Segment> lastSegmentChunks = last.chunks(chunk_size, align, twoBytes);
    std::vector<Segment> lastSegmentChunksFromPython = std::vector<Segment>{
        Segment(12288, 12528, hexStringToBytes("e01a0400000000000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00020a8000f13f2e008100610001007000000a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000020a800081ff2f008100610001007000000a88000080fa00000006000000fa004301a8000080fa00000006000000fa000028a9000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00420a8000f13f2e008100610001007000400a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb00"), 2),
        Segment(12528, 12768, hexStringToBytes("67006000420a800081ff2f008100610001007000400a88000080fa00000006000000fa004b01a8000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00820a8000f13f2e008100610001007000800a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000820a800081ff2f008100610001007000800a88000080fa00000006000000fa005301a8000080fa00000006000000fa0004a8a9000080fa00000006000200fa00000f78001e00780000407800674060000080fb00670060004a00dd00c20a8000f13f2e0081006100"), 2),
        Segment(12768, 13008, hexStringToBytes("01007000c00a88000080fa00000006000200fa00000f78001e00780000407800674060000080fb0067006000c20a800081ff2f008100610001007000c00a88000080fa00000006000000fa005b01a8000080fa00000006000600fa00004f78001147980012079800230798001e80fb00a1b9260000804000104078000074a1008080fb00f0072000008060007235800001f82f008100610001007000703588001e4090000080fb00a1b9260000804000104078000074a1008080fb00f0072000008060008235800001f82f008100610001007000803588003048070093480700f648070060470700700020004eff0700"), 2),
        Segment(13008, 13248, hexStringToBytes("7000200071ff07007000200090ff070070002000b3ff070064ff070088ff0700a8ff0700ccff070064ff0700a9ff07001e0090004fff07001e00900072ff07002e00900091ff07002e009000b4ff070050480700b348070016490700804707000080fa00000006000200fa00fb420700004f7800054d07001021a8001e407800e44f500002003a00b24b0700164107001e80fb00a1b9260000804000104078000074a1008080fb00f0072000008060003235800001f82f008100610001007000303588000b4d070010c0b3000080fa00000006000000fa00024d07000643070010c0b3000080fa0000000600f03fb100"), 2),
        Segment(13248, 13488, hexStringToBytes("0180b10006003500ee03090000000000403fb1000180b100fbff3d001000b000203fb00002003500008009000000000000000600ffff3700"), 2),
    };
    CHECK(lastSegmentChunks.size() == lastSegmentChunksFromPython.size());

    for (unsigned int i = 0; i < 5; i++)
    {
        // std::cout << "chunk number " << i << std::endl;
        CHECK(lastSegmentChunks[i].word_size_bytes == lastSegmentChunksFromPython[i].word_size_bytes);
        CHECK(lastSegmentChunks[i].minimum_address == lastSegmentChunksFromPython[i].minimum_address);
        CHECK(lastSegmentChunks[i].maximum_address == lastSegmentChunksFromPython[i].maximum_address);
        CHECK(lastSegmentChunks[i].data == lastSegmentChunksFromPython[i].data);
    }
}
