#pragma once

#include <unordered_map>
#include <memory>
#include <iostream>

#include "Protocol.h"

class GameServer;
class Session;

// ===== 패킷 핸들러 인터페이스 =====
class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    virtual void HandlePacket(
        std::shared_ptr<Session> session,
        const char* data,
        std::size_t size) = 0;
};

// ===== 패킷 디스패처 =====
class PacketDispatcher
{
public:
    void RegisterHandler(PACKET_ID pkt_id, std::unique_ptr<IPacketHandler> handler)
    {
        handlers_[pkt_id] = std::move(handler);
    }

    void DispatchPacket(std::shared_ptr<Session> session,
        const PACKET_HEADER& header,
        const char* data, std::size_t size)
    {
        auto it = handlers_.find(static_cast<PACKET_ID>(header.pkt_id));
        if (it != handlers_.end())
        {
            it->second->HandlePacket(session, data, size);
        }
        else
        {
            std::cout << "Unknown packet id: "
                << static_cast<uint16_t>(header.pkt_id) << std::endl;
        }
    }

private:
    std::unordered_map<PACKET_ID, std::unique_ptr<IPacketHandler>> handlers_;
};

//==================== 개별 핸들러들 ====================

class LoginHandler : public IPacketHandler
{
public:
    explicit LoginHandler(GameServer& server) : server_(server) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* data, std::size_t size) override;

private:
    GameServer& server_;
};

class LogoutHandler : public IPacketHandler
{
public:
    explicit LogoutHandler(GameServer& server) : server_(server) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* data, std::size_t size) override;

private:
    GameServer& server_;
};

class ChatMessageHandler : public IPacketHandler
{
public:
    explicit ChatMessageHandler(GameServer& server) : server_(server) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* data, std::size_t size) override;

private:
    GameServer& server_;
};

class CreateRoomHandler : public IPacketHandler
{
public:
    explicit CreateRoomHandler(GameServer& server) : server_(server) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* data, std::size_t size) override;

private:
    GameServer& server_;
};

class EnterRoomHandler : public IPacketHandler
{
public:
    explicit EnterRoomHandler(GameServer& server) : server_(server) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* data, std::size_t size) override;

private:
    GameServer& server_;
};

class LeaveRoomHandler : public IPacketHandler
{
public:
    explicit LeaveRoomHandler(GameServer& server) : server_(server) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* data, std::size_t size) override;

private:
    GameServer& server_;
};

class RoomListHandler : public IPacketHandler
{
public:
    explicit RoomListHandler(GameServer& server) : server_(server) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* data, std::size_t size) override;

private:
    GameServer& server_;
};

class PlayerStateHandler : public IPacketHandler
{
public:
    explicit PlayerStateHandler(GameServer& server) : server_(server) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* data, std::size_t size) override;

private:
    GameServer& server_;
};

