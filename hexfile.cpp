#include "doctest.h"

#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm> //std::remove

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

HexFile::HexFile() : word_size_bytes(0), execution_start_address(0)
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
        }
        else if (lineType == IHEX_END_OF_FILE)
        {
            // Pas de traitement spécial requis pour la fin de fichier
            std::cout << "IHEX_END_OF_FILE" << std::endl;
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

std::vector<Chunk> HexFile::chunked(std::string hexfile, BootAttrs bootattrs)
{
    std::vector<Chunk> res;

    std::ifstream file(hexfile);
    std::string line;

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

    word_size_bytes = 2;

    return res;
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
    return std::vector<Segment> {
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
TEST_CASE("chunked function debug_segments generation")
{
    BootAttrs bootattrs = defaultBootAttrsForTest();

    HexFile hex;
    std::vector<Chunk> chunks = hex.chunked(FLASH_HEX_FILE, bootattrs);

    CHECK(hex.debug_segments.size() == 128);

    std::vector<Segment> debugSegments = debugSegmentsFromPython();

    for(unsigned int i = 0; i < debugSegments.size(); i++) {
        CHECK(hex.debug_segments[i] == debugSegments[i]);
    }
}

// TEST_CASE("chunked function")
// {
//

// BootAttrs bootattrs = defaultBootAttrsForTest();
//     HexFile hex;

//     std::vector<Chunk> chunks = hex.chunked(FLASH_HEX_FILE, bootattrs);

//     std::vector<std::pair<unsigned int, std::string>> results{
//         std::make_pair(6144, "e0 1a 04 00 00 00 00 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 4a 00 dd 00 02 0a 80 00 f1 3f 2e 00 81 00 61 00 01 00 70 00 00 0a 88 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 02 0a 80 00 81 ff 2f 00 81 00 61 00 01 00 70 00 00 0a 88 00 00 80 fa 00 00 00 06 00 00 00 fa 00 43 01 a8 00 00 80 fa 00 00 00 06 00 00 00 fa 00 00 28 a9 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 4a 00 dd 00 42 0a 80 00 f1 3f 2e 00 81 00 61 00 01 00 70 00 40 0a 88 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00"),
//         std::make_pair(6264, "67 00 60 00 42 0a 80 00 81 ff 2f 00 81 00 61 00 01 00 70 00 40 0a 88 00 00 80 fa 00 00 00 06 00 00 00 fa 00 4b 01 a8 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 4a 00 dd 00 82 0a 80 00 f1 3f 2e 00 81 00 61 00 01 00 70 00 80 0a 88 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 82 0a 80 00 81 ff 2f 00 81 00 61 00 01 00 70 00 80 0a 88 00 00 80 fa 00 00 00 06 00 00 00 fa 00 53 01 a8 00 00 80 fa 00 00 00 06 00 00 00 fa 00 04 a8 a9 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 4a 00 dd 00 c2 0a 80 00 f1 3f 2e 00 81 00 61 00"),
//         std::make_pair(6384, "01 00 70 00 c0 0a 88 00 00 80 fa 00 00 00 06 00 02 00 fa 00 00 0f 78 00 1e 00 78 00 00 40 78 00 67 40 60 00 00 80 fb 00 67 00 60 00 c2 0a 80 00 81 ff 2f 00 81 00 61 00 01 00 70 00 c0 0a 88 00 00 80 fa 00 00 00 06 00 00 00 fa 00 5b 01 a8 00 00 80 fa 00 00 00 06 00 06 00 fa 00 00 4f 78 00 11 47 98 00 12 07 98 00 23 07 98 00 1e 80 fb 00 a1 b9 26 00 00 80 40 00 10 40 78 00 00 74 a1 00 80 80 fb 00 f0 07 20 00 00 80 60 00 72 35 80 00 01 f8 2f 00 81 00 61 00 01 00 70 00 70 35 88 00 1e 40 90 00 00 80 fb 00 a1 b9 26 00 00 80 40 00 10 40 78 00 00 74 a1 00 80 80 fb 00 f0 07 20 00 00 80 60 00 82 35 80 00 01 f8 2f 00 81 00 61 00 01 00 70 00 80 35 88 00 30 48 07 00 93 48 07 00 f6 48 07 00 60 47 07 00 70 00 20 00 4e ff 07 00"),
//         std::make_pair(6504, "70 00 20 00 71 ff 07 00 70 00 20 00 90 ff 07 00 70 00 20 00 b3 ff 07 00 64 ff 07 00 88 ff 07 00 a8 ff 07 00 cc ff 07 00 64 ff 07 00 a9 ff 07 00 1e 00 90 00 4f ff 07 00 1e 00 90 00 72 ff 07 00 2e 00 90 00 91 ff 07 00 2e 00 90 00 b4 ff 07 00 50 48 07 00 b3 48 07 00 16 49 07 00 80 47 07 00 00 80 fa 00 00 00 06 00 02 00 fa 00 fb 42 07 00 00 4f 78 00 05 4d 07 00 10 21 a8 00 1e 40 78 00 e4 4f 50 00 02 00 3a 00 b2 4b 07 00 16 41 07 00 1e 80 fb 00 a1 b9 26 00 00 80 40 00 10 40 78 00 00 74 a1 00 80 80 fb 00 f0 07 20 00 00 80 60 00 32 35 80 00 01 f8 2f 00 81 00 61 00 01 00 70 00 30 35 88 00 0b 4d 07 00 10 c0 b3 00 00 80 fa 00 00 00 06 00 00 00 fa 00 02 4d 07 00 06 43 07 00 10 c0 b3 00 00 80 fa 00 00 00 06 00 f0 3f b1 00"),
//         std::make_pair(6624, "01 80 b1 00 06 00 35 00 ee 03 09 00 00 00 00 00 40 3f b1 00 01 80 b1 00 fb ff 3d 00 10 00 b0 00 20 3f b0 00 02 00 35 00 00 80 09 00 00 00 00 00 00 00 06 00 ff ff 37 00")};

//     CHECK(chunks.size() == results.size());

//     for (unsigned int i = 0; i < chunks.size(); i++)
//     {
//         CHECK(chunks[i].address == results[i].first);
//         CHECK(bytesToHexString(chunks[i].data) == results[i].second);
//     }
// }
