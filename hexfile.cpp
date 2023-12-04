#include "doctest.h"

#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "hexfile.h"

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

unsigned int HexFile::crc_ihex(const std::vector<uint8_t> &bytes)
{
    unsigned int crc = 0;
    for (auto byte : bytes)
    {
        crc += byte;
    }
    crc &= 0xff;
    crc = ((~crc + 1) & 0xff);
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

std::vector<Chunk> HexFile::chunked(std::string hexfile, BootAttrs bootattrs)
{
    std::vector<Chunk> res;
    std::ifstream file(hexfile);
    std::string line;
    unsigned int extended_segment_address = 0;
    unsigned int extended_linear_address = 0;
    // unsigned int execution_start_address = 0;

    if (file.is_open())
    {
        std::cout << "file is open" << std::endl;
    }
    else
    {
        std::cout << "file is NOT open" << std::endl;
        return res;
    }

    unsigned int counter = 0;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] != ':')
        {
            throw std::runtime_error("Invalid format");
        }

        std::cout << " here new line" << std::endl;

        // Convert the line from hex to binary
        std::string binaryData;
        for (size_t i = 1; i < line.length(); i += 2)
        {
            std::string byteString = line.substr(i, 2);
            char byte = static_cast<char>(std::stoi(byteString, nullptr, 16));
            binaryData.push_back(byte);
        }
        // Extract the size, address, type, and data
        unsigned int size = static_cast<unsigned int>(binaryData[0]);
        unsigned int address = (static_cast<unsigned int>(binaryData[1]) << 8) | static_cast<unsigned int>(binaryData[2]);
        unsigned int type = static_cast<unsigned int>(binaryData[3]);

        std::cout << "line : " << line << std::endl;

        std::cout << "size : " << size << std::endl;
        std::cout << "address : " << address << std::endl;
        std::cout << "type : " << type << std::endl;

        if (type == IHEX_DATA) // Data record
        {
            std::vector<uint8_t> data(binaryData.begin() + 4, binaryData.begin() + 4 + size);
            Chunk chunk{address, data};
            res.push_back(chunk);
        }
        else if (type == IHEX_END_OF_FILE)
        {
            // Pas de traitement spécial requis pour la fin de fichier
        }
        else if (type == IHEX_EXTENDED_SEGMENT_ADDRESS)
        {
            extended_segment_address = static_cast<unsigned int>((binaryData[4] << 8) | binaryData[5]);
            extended_segment_address *= 16;
        }
        else if (type == IHEX_EXTENDED_LINEAR_ADDRESS)
        {
            extended_linear_address = static_cast<unsigned int>((binaryData[4] << 8) | binaryData[5]);
            extended_linear_address <<= 16;
        }
        else if (type == IHEX_START_SEGMENT_ADDRESS || type == IHEX_START_LINEAR_ADDRESS)
        {
            // execution_start_address = static_cast<unsigned int>((binaryData[4] << 8) | binaryData[5]);
        }
        else
        {
            throw std::runtime_error("Unexpected record type");
        }

        // Add CRC check and other record types handling as needed

        counter++;
        // if (counter == 4)
        // {
        //     break;
        // }
    }

    return res;
}

TEST_CASE("chunked function")
{
    std::string hexfile = "/home/yoctouser/mcbootflash-c/mcbootflash/tests/testcases/flash/test.hex";

    BootAttrs bootattrs;
    bootattrs.version = 258,
    bootattrs.max_packet_length = 256,
    bootattrs.device_id = 13398,
    bootattrs.erase_size = 2048,
    bootattrs.write_size = 8,
    bootattrs.memory_start = 6144;
    bootattrs.memory_end = 174080;
    bootattrs.has_checksum = true;

    HexFile hex;

    std::vector<Chunk> chunks = hex.chunked(hexfile, bootattrs);

    std::vector<std::pair<unsigned int, std::string>> results{
        std::make_pair(6144, "e0 1a 04 00 00 00 00 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 4a 00 dd 00 02 0a 80 00 f1 3f 2e 00 81 00 61 00 01 00 70 00 00 0a 88 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 02 0a 80 00 81 ff 2f 00 81 00 61 00 01 00 70 00 00 0a 88 00 00 80 fa 00 00 00 06 00 00 00 fa 00 43 01 a8 00 00 80 fa 00 00 00 06 00 00 00 fa 00 00 28 a9 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 4a 00 dd 00 42 0a 80 00 f1 3f 2e 00 81 00 61 00 01 00 70 00 40 0a 88 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00"),
        std::make_pair(6264, "67 00 60 00 42 0a 80 00 81 ff 2f 00 81 00 61 00 01 00 70 00 40 0a 88 00 00 80 fa 00 00 00 06 00 00 00 fa 00 4b 01 a8 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 4a 00 dd 00 82 0a 80 00 f1 3f 2e 00 81 00 61 00 01 00 70 00 80 0a 88 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 82 0a 80 00 81 ff 2f 00 81 00 61 00 01 00 70 00 80 0a 88 00 00 80 fa 00 00 00 06 00 00 00 fa 00 53 01 a8 00 00 80 fa 00 00 00 06 00 00 00 fa 00 04 a8 a9 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 4a 00 dd 00 c2 0a 80 00 f1 3f 2e 00 81 00 61 00"),
        std::make_pair(6384, "01 00 70 00 c0 0a 88 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 c2 0a 80 00 81 ff 2f 00 81 00 61 00 01 00 70 00 c0 0a 88 00 00 80 fa 00 00 00 06 00 00 00 fa 00 5b 01 a8 00 00 80 fa 00 00 00 06 00 06 00 fa 00 00 4f 78 00 11 47 98 00 12 07 98 00 23 07 98 00 1e 80 fb 00 a1 b9 26 00 00 80 40 00 10 40 78 00 00 74 a1 00 80 80 fb 00 f0 07 20 00 00 80 60 00 72 35 80 00 01 f8 2f 00 81 00 61 00 01 00 70 00 70 35 88 00 1e 40 90 00 00 80 fb 00 a1 b9 26 00 00 80 40 00 10 40 78 00 00 74 a1 00 80 80 fb 00 f0 07 20 00 00 80 60 00 82 35 80 00 01 f8 2f 00 81 00 61 00 01 00 70 00 80 35 88 00 30 48 07 00 93 48 07 00 f6 48 07 00 60 47 07 00 70 00 20 00 4e ff 07 00"),
        std::make_pair(6504, "70 00 20 00 71 ff 07 00 70 00 20 00 90 ff 07 00 70 00 20 00 b3 ff 07 00 64 ff 07 00 88 ff 07 00 a8 ff 07 00 cc ff 07 00 64 ff 07 00 a9 ff 07 00 1e 00 90 00 4f ff 07 00 1e 00 90 00 72 ff 07 00 2e 00 90 00 91 ff 07 00 2e 00 90 00 b4 ff 07 00 50 48 07 00 b3 48 07 00 16 49 07 00 80 47 07 00 00 80 fa 00 00 00 06 00 02 00 fa 00 fb 42 07 00 00 4f 78 00 05 4d 07 00 10 21 a8 00 1e 40 78 00 e4 4f 50 00 02 00 3a 00 b2 4b 07 00 16 41 07 00 1e 80 fb 00 a1 b9 26 00 00 80 40 00 10 40 78 00 00 74 a1 00 80 80 fb 00 f0 07 20 00 00 80 60 00 32 35 80 00 01 f8 2f 00 81 00 61 00 01 00 70 00 30 35 88 00 0b 4d 07 00 10 c0 b3 00 00 80 fa 00 00 00 06 00 00 00 fa 00 02 4d 07 00 06 43 07 00 10 c0 b3 00 00 80 fa 00 00 00 06 00 f0 3f b1 00"),
        std::make_pair(6624, "01 80 b1 00 06 00 35 00 ee 03 09 00 00 00 00 00 40 3f b1 00 01 80 b1 00 fb ff 3d 00 10 00 b0 00 20 3f b0 00 02 00 35 00 00 80 09 00 00 00 00 00 00 00 06 00 ff ff 37 00")};
    CHECK(chunks.size() == results.size());

    for (unsigned int i = 0; i < chunks.size(); i++)
    {
        CHECK(chunks[i].address == results[i].first);
        CHECK(bytesToHexString(chunks[i].data) == results[i].second);
    }
}
