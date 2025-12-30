#pragma once

#include <iostream>
#include <vector>
#include <deque>
#include <algorithm>
#include <string>
#include <memory>
#include <cstdio>
#include <boost/asio.hpp>

#include <google/protobuf/message.h>
#include "Protocol/Packet.pb.h"

#include "Session.h"
#include "SessionManager.h"
#include "UserManager.h"
#include "RoomManager.h"
#include "PacketHandler.h"

using boost::asio::ip::tcp;

class GameServer
{
public:
    GameServer(boost::asio::io_context& io_context)
        : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), PORT_NUMBER)),
        room_manager_(io_context)
    {
        InitializeMessageHandlers();

        //테스트 Room
        //room_manager_.CreateRoom("Official Room #1 (PVP Enabled)", "", false);
        room_manager_.StartRoomCountLogging(std::chrono::seconds(30));
        StartAccept();
    }

    void Start()
    {
        std::cout << "Game server started on port "
            << acceptor_.local_endpoint().port() << std::endl;
    }

    void Stop()
    {
        std::cout << "Stopping game server..." << std::endl;
        acceptor_.close();
        session_manager_.DisconnectAll();
    }



    void OnSessionDisconnected(std::shared_ptr<Session> session)
    {
        if (session->IsAuthenticated())
        {
            uint32_t user_id = session->GetUserId();
            auto user = user_manager_.GetUser(user_id);
            if (user)
            {
                user->SetOnline(false);

                auto rooms = room_manager_.GetRoomList();
                for (auto& room : rooms)
                {
                    room->RemoveUser(user_id);
                }

                std::cout << "User disconnected: " << user->GetUsername()
                    << " (ID: " << user_id << ")" << std::endl;
            }
        }
        session_manager_.RemoveSession(session);
    }

    PacketDispatcher& GetPacketDispatcher() { return packet_dispatcher_; }

private:
    void StartAccept()
    {
        auto new_session = std::make_shared<Session>(
            tcp::socket(io_context_), *this);

        acceptor_.async_accept(new_session->GetSocket(),
            [this, new_session](boost::system::error_code ec)
            {
                if (!ec)
                {
                    session_manager_.AddSession(new_session);
                    new_session->Start();
                }
                StartAccept();
            });
    }

    void InitializeMessageHandlers()
    {
        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_LOGIN,
            std::make_unique<LoginHandler>(user_manager_));

        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_GUEST_LOGIN,
            std::make_unique<GuestLoginHandler>(user_manager_));

        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_LOGOUT,
            std::make_unique<LogoutHandler>(user_manager_, room_manager_));

        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_CREATE_ROOM,
            std::make_unique<CreateRoomHandler>(user_manager_, room_manager_));

        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_LEAVE_ROOM,
            std::make_unique<LeaveRoomHandler>(user_manager_, room_manager_));

        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_ROOM_LIST,
            std::make_unique<RoomListHandler>(room_manager_));

        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_CHAT,
            std::make_unique<ChatMessageHandler>(user_manager_, room_manager_));

        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_ENTER_ROOM,
            std::make_unique<EnterRoomHandler>(user_manager_, room_manager_));

        packet_dispatcher_.RegisterHandler(
            fixer::PacketId::REQ_PLAYER_STATE,
            std::make_unique<PlayerStateHandler>(user_manager_, room_manager_));
    }



    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;

    SessionManager session_manager_;
    UserManager    user_manager_;
    RoomManager    room_manager_;
    PacketDispatcher packet_dispatcher_;
};
