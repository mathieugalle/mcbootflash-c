#include <string.h> // memcpy
#include <iostream>
#include <sstream>
#include <iomanip>
#include <array>
#include <vector>

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
