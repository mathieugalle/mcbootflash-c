#include <string.h> // memcpy
#include <iostream>
#include <sstream>
#include <iomanip>
#include <array>
#include <vector>
#include <fstream>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

template <std::size_t N>
std::string arrayToHexString(const std::array<uint8_t, N> &arr)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < N; ++i)
    {
        ss << std::setw(2) << static_cast<unsigned>(arr[i]);
        if (i < N - 1)
        {
            ss << " ";
        }
    }
    return ss.str();
}

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

/// @brief Sent by the bootloader in response to a command.
enum ResponseCode
{
    SUCCESS = 0x01,
    UNSUPPORTED_COMMAND = 0xFF,
    BAD_ADDRESS = 0xFE,
    BAD_LENGTH = 0xFD,
    VERIFY_FAIL = 0xFC
};

/// @brief The MCC 16-bit bootloader supports these commands.
enum CommandCode
{
    READ_VERSION = 0x00,
    READ_FLASH = 0x01,
    WRITE_FLASH = 0x02,
    ERASE_FLASH = 0x03,
    CALC_CHECKSUM = 0x08,
    RESET_DEVICE = 0x09,
    SELF_VERIFY = 0x0A,
    GET_MEMORY_ADDRESS_RANGE = 0x0B,
};

class Packet
{
private:
    uint8_t command;
    uint16_t data_length;
    uint32_t unlock_sequence;
    uint32_t address;

public:
    Packet(uint8_t command, uint16_t data_length = 0, uint32_t unlock_sequence = 0, uint32_t address = 0)
        : command(command), data_length(data_length), unlock_sequence(unlock_sequence), address(address) {}

    std::array<uint8_t, 11> toBytes() const
    {
        std::array<uint8_t, 11> buffer;
        uint8_t *bufPtr = buffer.data();
        memcpy(bufPtr, &command, sizeof(command));
        bufPtr += sizeof(command);
        memcpy(bufPtr, &data_length, sizeof(data_length));
        bufPtr += sizeof(data_length);
        memcpy(bufPtr, &unlock_sequence, sizeof(unlock_sequence));
        bufPtr += sizeof(unlock_sequence);
        memcpy(bufPtr, &address, sizeof(address));
        return buffer;
    }
    // Déserialisation
    void fromBytes(const std::array<uint8_t, 11> &buffer)
    {
        uint8_t command;
        uint16_t data_length;
        uint32_t unlock_sequence;
        uint32_t address;

        const uint8_t *bufPtr = buffer.data();

        memcpy(&command, bufPtr, sizeof(command));
        bufPtr += sizeof(command);

        memcpy(&data_length, bufPtr, sizeof(data_length));
        bufPtr += sizeof(data_length);

        memcpy(&unlock_sequence, bufPtr, sizeof(unlock_sequence));
        bufPtr += sizeof(unlock_sequence);

        memcpy(&address, bufPtr, sizeof(address));

        this->command = command;
        this->data_length = data_length;
        this->unlock_sequence = unlock_sequence;
        this->address = address;
    }
    static size_t getSize()
    {
        return sizeof(uint8_t) + sizeof(uint16_t) + 2 * sizeof(uint32_t);
    }
};

TEST_CASE("Packet class creation, fromBytes, toBytes")
{
    Packet p(CommandCode::GET_MEMORY_ADDRESS_RANGE);

    // std::cout << arrayToHexString(p.toBytes()) << std::endl;

    CHECK(arrayToHexString(p.toBytes()) == "0b 00 00 00 00 00 00 00 00 00 00");
}

typedef Packet ResponseBase;
typedef Packet Command;

class Version : public ResponseBase
{
private:
    uint16_t version;
    uint16_t max_packet_length;
    uint16_t device_id;
    uint16_t erase_size;
    uint16_t write_size;

public:
    Version(
        uint8_t command,
        uint16_t data_length = 0,
        uint32_t unlock_sequence = 0,
        uint32_t address = 0,
        uint16_t version = 0,
        uint16_t max_packet_length = 0,
        uint16_t device_id = 0,
        uint16_t erase_size = 0,
        uint16_t write_size = 0)
        : ResponseBase(command, data_length, unlock_sequence, address),
          version(version),
          max_packet_length(max_packet_length),
          device_id(device_id),
          erase_size(erase_size),
          write_size(write_size)
    {
    }

    std::array<uint8_t, 37> toBytes() const
    {
        std::array<uint8_t, 11> baseBuffer = ResponseBase::toBytes();

        // la liste d'initialisation vide {} met tous les octets à 0 pour la répétabilité lors des tests
        std::array<uint8_t, 37> buffer{};
        memcpy(buffer.data(), baseBuffer.data(), baseBuffer.size());

        // On copie chaque champ dans le buffer, en ignorant les champs non utilisés
        memcpy(buffer.data() + baseBuffer.size(), &version, sizeof(version));
        memcpy(buffer.data() + baseBuffer.size() + 2, &max_packet_length, sizeof(max_packet_length));
        memcpy(buffer.data() + baseBuffer.size() + 6, &device_id, sizeof(device_id));
        memcpy(buffer.data() + baseBuffer.size() + 10, &erase_size, sizeof(erase_size));
        memcpy(buffer.data() + baseBuffer.size() + 12, &write_size, sizeof(write_size));
        return buffer;
    }

    void fromBytes(const std::array<uint8_t, 37> &buffer)
    {
        std::array<uint8_t, 11> baseBuffer;
        memcpy(baseBuffer.data(), buffer.data(), baseBuffer.size());
        ResponseBase::fromBytes(baseBuffer);

        memcpy(&version, buffer.data() + baseBuffer.size(), sizeof(version));
        memcpy(&max_packet_length, buffer.data() + baseBuffer.size() + 2, sizeof(max_packet_length));
        memcpy(&device_id, buffer.data() + baseBuffer.size() + 6, sizeof(device_id));
        memcpy(&erase_size, buffer.data() + baseBuffer.size() + 10, sizeof(erase_size));
        memcpy(&write_size, buffer.data() + baseBuffer.size() + 12, sizeof(write_size));
    }

    static size_t getSize()
    {
        return ResponseBase::getSize() + 26; // il y a des octets ignorés
    }
};

TEST_CASE("Version class getSize ?")
{
    Version v(ResponseCode::SUCCESS);
    CHECK(v.getSize() == 37);
}

TEST_CASE("Version class SUCCESS simple")
{
    Version v((uint8_t)ResponseCode::SUCCESS);
    CHECK(arrayToHexString(v.toBytes()) == "01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");
}
TEST_CASE("Version class SUCCESS avec valeurs différentes")
{
    Version v((uint8_t)ResponseCode::SUCCESS,
              0, 0, 0, 42, 43, 45, 48, 34);
    CHECK(arrayToHexString(v.toBytes()) == "01 00 00 00 00 00 00 00 00 00 00 2a 00 2b 00 00 00 2d 00 00 00 30 00 22 00 00 00 00 00 00 00 00 00 00 00 00 00");
}

class Response : public Packet
{
private:
    ResponseCode success;

public:
    Response(uint8_t command, uint16_t data_length = 0, uint32_t unlock_sequence = 0, uint32_t address = 0, ResponseCode success = ResponseCode::UNSUPPORTED_COMMAND)
        : Packet(command, data_length, unlock_sequence, address), success(success) {}

    std::array<uint8_t, 12> toBytes() const
    {
        std::array<uint8_t, 11> baseBuffer = Packet::toBytes();
        std::array<uint8_t, 12> buffer;
        std::copy(baseBuffer.begin(), baseBuffer.end(), buffer.begin());
        buffer[11] = static_cast<uint8_t>(success);
        return buffer;
    }

    void fromBytes(const std::array<uint8_t, 12> &buffer)
    {
        std::array<uint8_t, 11> baseBuffer;
        std::copy(buffer.begin(), buffer.begin() + 11, baseBuffer.begin());
        Packet::fromBytes(baseBuffer);
        success = static_cast<ResponseCode>(buffer[11]);
    }

    static size_t getSize()
    {
        return Packet::getSize() + sizeof(ResponseCode);
    }
};

TEST_CASE("Response class BAD_LENGTH")
{
    Response r((uint8_t)ResponseCode::BAD_LENGTH);
    CHECK(arrayToHexString(r.toBytes()) == "fd 00 00 00 00 00 00 00 00 00 00 ff");
}
TEST_CASE("Response class BAD_ADDRESS")
{
    Response r((uint8_t)ResponseCode::BAD_ADDRESS);
    CHECK(arrayToHexString(r.toBytes()) == "fe 00 00 00 00 00 00 00 00 00 00 ff");
}

/// @brief Response to `GET_MEMORY_RANGE` command.
/*
    Layout::
        | [Response] | uint32        | uint32      |
        | [Response] | program_start | program_end |
    Parameters
    ----------
    program_start : uint32
        Low end of address space to which application firmware can be flashed.
    program_end : uint32
        High end of address space to which application firmware can be flashed.
*/
class MemoryRange : public Response
{
private:
    uint32_t program_start;
    uint32_t program_end;

public:
    MemoryRange(
        uint8_t command,
        uint16_t data_length = 0,
        uint32_t unlock_sequence = 0,
        uint32_t address = 0,
        ResponseCode success = ResponseCode::UNSUPPORTED_COMMAND)
        : Response(command, data_length, unlock_sequence, address, success), program_start(0), program_end(0)
    {
    }
    std::array<uint8_t, 18> toBytes() const
    {
        std::array<uint8_t, 12> baseBuffer = Response::toBytes();
        std::array<uint8_t, 18> buffer;
        // destination : buffer. source : baseBuffer, nombre à copier : size
        memcpy(buffer.data(), baseBuffer.data(), baseBuffer.size());
        // Copier program_start dans buffer
        memcpy(buffer.data() + baseBuffer.size(), &program_start, sizeof(program_start));
        // Copier program_end dans buffer
        memcpy(buffer.data() + baseBuffer.size() + sizeof(program_start), &program_end, sizeof(program_end));
        return buffer;
    }
    void fromBytes(const std::array<uint8_t, 18> &buffer)
    {
        std::array<uint8_t, 12> baseBuffer;
        // Copier les premiers 12 octets de buffer dans baseBuffer
        memcpy(baseBuffer.data(), buffer.data(), baseBuffer.size());
        Response::fromBytes(baseBuffer);

        // Copier program_start depuis buffer
        memcpy(&program_start, buffer.data() + baseBuffer.size(), sizeof(program_start));

        // Copier program_end depuis buffer
        memcpy(&program_end, buffer.data() + baseBuffer.size() + sizeof(program_start), sizeof(program_end));
    }
    static size_t getSize()
    {
        return Response::getSize() + sizeof(program_start) + sizeof(program_end);
    }
};

TEST_CASE("MemoryRange class")
{
    Response r((uint8_t)ResponseCode::BAD_LENGTH);
    CHECK(arrayToHexString(r.toBytes()) == "fd 00 00 00 00 00 00 00 00 00 00 ff");
}
TEST_CASE("MemoryRange class 2")
{
    Response r((uint8_t)ResponseCode::BAD_ADDRESS);
    CHECK(arrayToHexString(r.toBytes()) == "fe 00 00 00 00 00 00 00 00 00 00 ff");
}

class Checksum : public Response
{
private:
    uint16_t checksum;

public:
    Checksum(
        uint8_t command,
        uint16_t checksum = 0)
        : Response(command), checksum(checksum)
    {
    }

    std::array<uint8_t, 14> toBytes() const
    {
        std::array<uint8_t, 12> baseBuffer = Response::toBytes();
        std::array<uint8_t, 14> buffer;
        memcpy(buffer.data(), baseBuffer.data(), baseBuffer.size());
        memcpy(buffer.data() + baseBuffer.size(), &checksum, sizeof(checksum));
        return buffer;
    }

    void fromBytes(const std::array<uint8_t, 14> &buffer)
    {
        std::array<uint8_t, 12> baseBuffer;
        memcpy(baseBuffer.data(), buffer.data(), baseBuffer.size());
        Response::fromBytes(baseBuffer);
        memcpy(&checksum, buffer.data() + baseBuffer.size(), sizeof(checksum));
    }

    static size_t getSize()
    {
        return Response::getSize() + sizeof(checksum);
    }
};

TEST_CASE("Checksum class is 42")
{
    Checksum c(ResponseCode::SUCCESS, 42);
    CHECK(arrayToHexString(c.toBytes()) == "01 00 00 00 00 00 00 00 00 00 00 ff 2a 00");
}
TEST_CASE("Checksum class is 78")
{
    Checksum c(ResponseCode::BAD_ADDRESS, 78);
    CHECK(arrayToHexString(c.toBytes()) == "fe 00 00 00 00 00 00 00 00 00 00 ff 4e 00");
}

/**
 * Bootloader attributes.
 *
 * Attributes:
 *   version - Bootloader version number.
 *   max_packet_length - Maximum number of bytes which can be sent to the bootloader
 *                       per packet. Includes the size of the packet itself plus
 *                       associated data.
 *   device_id - A device-specific identifier.
 *   erase_size - Size of a flash erase page in bytes. When erasing flash, the size
 *                of the memory area which should be erased is given in number of
 *                erase pages.
 *   write_size - Size of a write block in bytes. When writing to flash, the data
 *                must align with a write block.
 *   memory_start - Start address of the program memory range.
 *   memory_end - End address of the program memory range. The range is half-open,
 *                i.e., this address is not part of the program memory range.
 *   has_checksum - Indicates whether or not the bootloader supports the `CALC_CHECKSUM`
 *                  command.
 */
typedef struct
{
    int version;
    int max_packet_length;
    int device_id;
    int erase_size;
    int write_size;
    int memory_start;
    int memory_end;
    bool has_checksum;
} BootAttrs;

struct Chunk
{
    unsigned int address;
    std::vector<uint8_t> data;
};

// Intel hex types.
#define IHEX_DATA 0
#define IHEX_END_OF_FILE 1
#define IHEX_EXTENDED_SEGMENT_ADDRESS 2
#define IHEX_START_SEGMENT_ADDRESS 3
#define IHEX_EXTENDED_LINEAR_ADDRESS 4
#define IHEX_START_LINEAR_ADDRESS 5

unsigned int crc_ihex(const std::vector<uint8_t> &bytes)
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

struct Segment {
    unsigned int minimum_address;
    unsigned int maximum_address;
    std::vector<uint8_t> data;
    unsigned int word_size_bytes;
};


void unpack_ihex(const std::string &record, unsigned int &type_, unsigned int &address, unsigned int &size, std::vector<uint8_t> &data)
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

std::vector<Chunk> chunked(std::string hexfile, BootAttrs bootattrs)
{
    std::vector<Chunk> res;
    std::ifstream file(hexfile);
    std::string line;
    unsigned int extended_segment_address = 0;
    unsigned int extended_linear_address = 0;
    unsigned int execution_start_address = 0;

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
            execution_start_address = static_cast<unsigned int>((binaryData[4] << 8) | binaryData[5]);
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
    std::string hexfile = "../mcbootflash/tests/testcases/flash/test.hex";

    BootAttrs bootattrs;
    bootattrs.version = 258,
    bootattrs.max_packet_length = 256,
    bootattrs.device_id = 13398,
    bootattrs.erase_size = 2048,
    bootattrs.write_size = 8,
    bootattrs.memory_start = 6144;
    bootattrs.memory_end = 174080;
    bootattrs.has_checksum = true;

    std::vector<Chunk> chunks = chunked(hexfile, bootattrs);

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
