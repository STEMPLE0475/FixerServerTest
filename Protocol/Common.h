#pragma once

#include <cstdint>

constexpr std::uint16_t PORT_NUMBER = 31452;
constexpr std::uint16_t MAX_RECEIVE_BUFFER_LEN = 2048;

constexpr std::uint16_t MAX_ID_LEN = 32;
constexpr std::uint16_t MAX_PW_LEN = 32;
constexpr std::uint16_t MAX_NAME_LEN = 32;
constexpr std::uint16_t MAX_ROOM_NAME_LEN = 32;
constexpr std::uint16_t MAX_MESSAGE_LEN = 128;

constexpr std::uint16_t MAX_PLAYERS_PER_ROOM = 16;

#pragma pack(push, 1)
struct PACKET_HEADER
{
    std::uint16_t pkt_id;
    std::uint16_t pkt_size;
};
#pragma pack(pop)

