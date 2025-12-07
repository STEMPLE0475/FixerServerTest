#include "PacketHandler.h"
#include "Session.h"
#include "GameServer.h"
#include "Protocol.h"

// ===== Login =====
void LoginHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* data, std::size_t size)
{
    if (size < sizeof(PKT_REQ_LOGIN))
        return;

    const auto& request = *reinterpret_cast<const PKT_REQ_LOGIN*>(data);
    server_.ProcessLogin(session, request);
}

// ===== Logout =====
void LogoutHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* data, std::size_t size)
{
    if (size < sizeof(PKT_REQ_LOGOUT))
        return;

    const auto& request = *reinterpret_cast<const PKT_REQ_LOGOUT*>(data);
    server_.ProcessLogout(session, request);
}

// ===== Chat =====
void ChatMessageHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* data, std::size_t size)
{
    if (!session->IsAuthenticated())
        return;

    if (size < sizeof(PKT_REQ_CHAT))
        return;

    const auto& message = *reinterpret_cast<const PKT_REQ_CHAT*>(data);
    server_.ProcessChatMessage(session, message);
}

// ===== Room Create =====
void CreateRoomHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* data, std::size_t size)
{
    if (size < sizeof(PKT_REQ_CREATE_ROOM))
        return;

    const auto& request = *reinterpret_cast<const PKT_REQ_CREATE_ROOM*>(data);
    server_.ProcessCreateRoom(session, request);
}

// ===== Room Enter =====
void EnterRoomHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* data, std::size_t size)
{
    if (size < sizeof(PKT_REQ_ENTER_ROOM))
        return;

    const auto& request = *reinterpret_cast<const PKT_REQ_ENTER_ROOM*>(data);
    server_.ProcessEnterRoom(session, request);
}

// ===== Room Leave =====
void LeaveRoomHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* data, std::size_t size)
{
    if (size < sizeof(PKT_REQ_LEAVE_ROOM))
        return;

    const auto& request = *reinterpret_cast<const PKT_REQ_LEAVE_ROOM*>(data);
    server_.ProcessLeaveRoom(session, request);
}

// ===== Room List =====
void RoomListHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* data, std::size_t size)
{
    if (size < sizeof(PKT_REQ_ROOM_LIST))
        return;

    const auto& request = *reinterpret_cast<const PKT_REQ_ROOM_LIST*>(data);
    server_.ProcessRoomListRequest(session, request);
}

// ===== Player Input =====
void PlayerStateHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* data, std::size_t size)
{
    if (size < sizeof(PKT_REQ_PLAYER_STATE))
        return;

    const auto& request = *reinterpret_cast<const PKT_REQ_PLAYER_STATE*>(data);
    server_.ProcessPlayerState(session, request);
}
