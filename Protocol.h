#pragma once

#include <cstdint>
#include <cstring>

constexpr std::uint16_t PORT_NUMBER = 31452;
constexpr std::uint16_t MAX_RECEIVE_BUFFER_LEN = 512;

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

enum PACKET_ID : std::uint16_t {
    PACKET_ID_INVALID = 0,

    REQ_LOGIN = 10,
    RES_LOGIN = 11,
    REQ_LOGOUT = 12,
    RES_LOGOUT = 13,

    REQ_CREATE_ROOM = 20,
    RES_CREATE_ROOM = 21,

    REQ_ENTER_ROOM = 22,
    RES_ENTER_ROOM = 23,
    REQ_LEAVE_ROOM = 24,
    RES_LEAVE_ROOM = 25,

    NOTICE_ROOM_INFO = 26,

    RES_ROOM_LIST = 27,
    REQ_ROOM_LIST = 28,

    REQ_CHAT = 30,
    NOTICE_CHAT = 31,

    REQ_PLAYER_STATE = 40,
    NOTICE_PLAYER_STATE = 41,

    NOTICE_GAME_CLEAR = 50,
};

struct CharacterState
{
    float pos_x;
    float pos_y;
};

// 로그인 요청
struct PKT_REQ_LOGIN : public PACKET_HEADER
{
    char userId[MAX_ID_LEN];
    char password[MAX_PW_LEN];
};

// 로그인 응답
struct PKT_RES_LOGIN : public PACKET_HEADER
{
    uint32_t userId;
    bool isSuccess;
};

// 로그아웃 요청
struct PKT_REQ_LOGOUT : public PACKET_HEADER
{
    char userId[MAX_ID_LEN];
};

// 로그아웃 응답
struct PKT_RES_LOGOUT : public PACKET_HEADER
{
    bool isSuccess;
};

// 방 생성 요청
struct PKT_REQ_CREATE_ROOM : public PACKET_HEADER
{
    char roomName[MAX_ROOM_NAME_LEN];
};

// 방 생성 응답
struct PKT_RES_CREATE_ROOM : public PACKET_HEADER
{
    bool isSuccess;
};

// 방 입장 요청
struct PKT_REQ_ENTER_ROOM : public PACKET_HEADER
{
    char roomName[MAX_ROOM_NAME_LEN];
};

// 방 입장 응답
struct PKT_RES_ENTER_ROOM : public PACKET_HEADER
{
    bool isSuccess;
};

// 방 퇴장 요청
struct PKT_REQ_LEAVE_ROOM : public PACKET_HEADER
{
    char roomName[MAX_ROOM_NAME_LEN];
};

// 방 퇴장 응답
struct PKT_RES_LEAVE_ROOM : public PACKET_HEADER
{
    bool isSuccess;
};

// 방 정보 브로드캐스트
struct PKT_NOTICE_ROOM_INFO : public PACKET_HEADER
{
    char roomName[MAX_ROOM_NAME_LEN]; 
    std::uint16_t playerCount;
};

// 방 리스트 요청
struct PKT_REQ_ROOM_LIST : public PACKET_HEADER
{
    // 요청 시 별도 데이터 필요 없음
};

// 방 리스트 응답
struct PKT_RES_ROOM_LIST : public PACKET_HEADER
{
    // 예시: 최대 10개 방 정보 반환
    struct RoomInfo {
        char roomName[MAX_ROOM_NAME_LEN];
        std::uint16_t playerCount;
    };
    RoomInfo rooms[10];
    std::uint16_t roomCount;
};

// 채팅 요청
struct PKT_REQ_CHAT : public PACKET_HEADER
{
    char message[MAX_MESSAGE_LEN];
};

// 채팅 브로드캐스트
struct PKT_NOTICE_CHAT : public PACKET_HEADER
{
    char senderName[MAX_NAME_LEN];
    char message[MAX_MESSAGE_LEN];
};

// 플레이어 상태 전송
struct PKT_REQ_PLAYER_STATE : public PACKET_HEADER
{
    CharacterState characterState;
};

struct PlayerStateEntry {
    std::uint32_t userId;
    CharacterState state;
};

// 플레이어 상태 브로드캐스트
struct PKT_NOTICE_PLAYER_STATE : public PACKET_HEADER
{
    std::uint16_t count;
    PlayerStateEntry players[MAX_PLAYERS_PER_ROOM];
};

// 게임 클리어 브로드캐스트
struct PKT_NOTICE_GAME_CLEAR : public PACKET_HEADER
{
    char winnerName[MAX_NAME_LEN];
};



#pragma pack(pop)

