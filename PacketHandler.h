#pragma once

#include <unordered_map>
#include <memory>
#include <iostream>

#include "Protocol/Packet.pb.h"
#include "Protocol/Common.h"

class Session;
class UserManager;
class RoomManager;

class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    virtual void HandlePacket(
        std::shared_ptr<Session> session,
        const char* body_data,
        std::size_t body_size) = 0;
};

class PacketDispatcher
{
public:
    void RegisterHandler(fixer::PacketId pkt_id, std::unique_ptr<IPacketHandler> handler)
    {
        handlers_[static_cast<std::uint16_t>(pkt_id)] = std::move(handler);
    }

    void DispatchPacket(std::shared_ptr<Session> session,
        const PACKET_HEADER& header,
        const char* body, std::size_t body_size)
    {
        auto it = handlers_.find(header.pkt_id);
        if (it != handlers_.end())
        {
            it->second->HandlePacket(session, body, body_size);
        }
        else
        {
            std::cout << "Unknown packet id: " << static_cast<uint16_t>(header.pkt_id) << std::endl;
        }
    }

private:
    std::unordered_map<std::uint16_t, std::unique_ptr<IPacketHandler>> handlers_;
};

// 개별 핸들러

class LoginHandler : public IPacketHandler
{
public:
    explicit LoginHandler(UserManager& users_) : users_(users_) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body_data, std::size_t body_size) override;

private:
    UserManager& users_;
};

class GuestLoginHandler : public IPacketHandler
{
public:
    explicit GuestLoginHandler(UserManager& users_) : users_(users_) {}

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body_data, std::size_t body_size) override;

private:
    UserManager& users_;
};

class LogoutHandler : public IPacketHandler
{
public:
    LogoutHandler(UserManager& users, RoomManager& rooms)
        : users_(users), rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body,
        std::size_t body_size) override;

private:
    UserManager& users_;
    RoomManager& rooms_;
};

class ChatMessageHandler : public IPacketHandler
{
public:
    ChatMessageHandler(UserManager& users, RoomManager& rooms)
        : users_(users), rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body, std::size_t body_size) override;

private:
    UserManager& users_;
    RoomManager& rooms_;
};

class CreateRoomHandler : public IPacketHandler
{
public:
    CreateRoomHandler(UserManager& users, RoomManager& rooms)
        : users_(users), rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body,
        std::size_t body_size) override;

private:
    UserManager& users_;
    RoomManager& rooms_;
};

class EnterRoomHandler : public IPacketHandler
{
public:
    EnterRoomHandler(UserManager& users, RoomManager& rooms)
        : users_(users), rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body, std::size_t body_size) override;

private:
    UserManager& users_;
    RoomManager& rooms_;
};

class LeaveRoomHandler : public IPacketHandler
{
public:
    LeaveRoomHandler(UserManager& users, RoomManager& rooms)
        : users_(users), rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body,
        std::size_t body_size) override;

private:
    UserManager& users_;
    RoomManager& rooms_;
};

class RoomListHandler : public IPacketHandler
{
public:
    explicit RoomListHandler(RoomManager& rooms)
        : rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body,
        std::size_t body_size) override;

private:
    RoomManager& rooms_;
};

class PlayerStateHandler : public IPacketHandler
{
public:
    PlayerStateHandler(UserManager& users, RoomManager& rooms)
        : users_(users), rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body,
        std::size_t body_size) override;

private:
    UserManager& users_;
    RoomManager& rooms_;
};

class RoomPlayerNameHandler : public IPacketHandler
{
public:
    RoomPlayerNameHandler(UserManager& users, RoomManager& rooms)
        : users_(users), rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body,
        std::size_t body_size) override;

private:
    UserManager& users_;
    RoomManager& rooms_;
};

class PlayerInteractHandler : public IPacketHandler
{
public:
    PlayerInteractHandler(UserManager& users, RoomManager& rooms)
        : users_(users), rooms_(rooms) {
    }

    void HandlePacket(std::shared_ptr<Session> session,
        const char* body,
        std::size_t body_size) override;

private:
    UserManager& users_;
    RoomManager& rooms_;
};