#pragma once

#include <iostream>
#include <vector>
#include <deque>
#include <algorithm>
#include <string>
#include <memory>
#include <cstdio>
#include <boost/asio.hpp>

#include "Session.h"
#include "Protocol.h"
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

        // 기본 방 몇 개 생성 (원하면 이름만 바꿔도 됨)
        room_manager_.CreateRoom("Lobby", 100);
        room_manager_.CreateRoom("Room1", 4);

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

    // ===== 패킷 처리들 =====
    void ProcessLogin(std::shared_ptr<Session> session, const PKT_REQ_LOGIN& request)
    {
        PKT_RES_LOGIN response{};
        response.pkt_id = RES_LOGIN;
        response.pkt_size = sizeof(PKT_RES_LOGIN);

        if (request.userId[0] != '\0' && request.password[0] != '\0')
        {
            auto user = user_manager_.CreateUser(request.userId);
            if (user)
            {
                user->SetOnline(true);
                user->SetSession(session);

                session->SetUserId(user->GetId());
                session->SetAuthenticated(true);

                response.userId = session->GetUserId();
                response.isSuccess = true;
                std::cout << "User logged in: " << request.userId
                    << " (ID: " << user->GetId() << ")" << std::endl;
            }
            else
            {
                // 중복 아이디
                response.isSuccess = false;
            }
        }
        else
        {
            response.isSuccess = false;
        }

        session->SendMessage(&response, sizeof(response));
    }

    void ProcessLogout(std::shared_ptr<Session> session, const PKT_REQ_LOGOUT& request)
    {
        PKT_RES_LOGOUT response{};
        response.pkt_id = RES_LOGOUT;
        response.pkt_size = sizeof(PKT_RES_LOGOUT);
        response.isSuccess = false;

        if (!session->IsAuthenticated())
        {
            session->SendMessage(&response, sizeof(response));
            return;
        }

        uint32_t user_id = session->GetUserId();
        auto user = user_manager_.GetUser(user_id);
        if (user)
        {
            // 모든 방에서 제거
            auto rooms = room_manager_.GetRoomList();
            for (auto& room : rooms)
            {
                room->RemoveUser(user_id);
            }

            user->SetOnline(false);
            session->SetAuthenticated(false);
            response.isSuccess = true;

            std::cout << "User logout: " << user->GetUsername()
                << " (ID: " << user_id << ")" << std::endl;
        }

        session->SendMessage(&response, sizeof(response));
    }

    void ProcessChatMessage(std::shared_ptr<Session> session, const PKT_REQ_CHAT& message)
    {
        if (!session->IsAuthenticated())
            return;

        auto user = user_manager_.GetUser(session->GetUserId());
        if (!user)
            return;

        PKT_NOTICE_CHAT notice{};
        notice.pkt_id = NOTICE_CHAT;
        notice.pkt_size = sizeof(PKT_NOTICE_CHAT);

        std::snprintf(notice.senderName, MAX_NAME_LEN, "%s", user->GetUsername().c_str());
        std::snprintf(notice.message, MAX_MESSAGE_LEN, "%s", message.message);

        // 일단은 전체 브로드캐스트 (원하면 방 단위로 바꿔도 됨)
        session_manager_.BroadcastToAll(&notice, sizeof(notice));
    }

    void ProcessCreateRoom(std::shared_ptr<Session> session, const PKT_REQ_CREATE_ROOM& request)
    {
        PKT_RES_CREATE_ROOM response{};
        response.pkt_id = RES_CREATE_ROOM;
        response.pkt_size = sizeof(PKT_RES_CREATE_ROOM);
        response.isSuccess = false;

        if (!session->IsAuthenticated())
        {
            session->SendMessage(&response, sizeof(response));
            return;
        }

        std::string roomName(request.roomName);
        if (roomName.empty())
        {
            session->SendMessage(&response, sizeof(response));
            return;
        }

        auto exist = room_manager_.GetRoomByName(roomName);
        if (exist)
        {
            // 이미 존재
            session->SendMessage(&response, sizeof(response));
            return;
        }

        room_manager_.CreateRoom(roomName, 4);
        response.isSuccess = true;

        std::cout << "Room created by user "
            << session->GetUserId()
            << ": " << roomName << std::endl;

        session->SendMessage(&response, sizeof(response));
    }

    void ProcessEnterRoom(std::shared_ptr<Session> session, const PKT_REQ_ENTER_ROOM& request)
    {
        PKT_RES_ENTER_ROOM response{};
        response.pkt_id = RES_ENTER_ROOM;
        response.pkt_size = sizeof(PKT_RES_ENTER_ROOM);
        response.isSuccess = false;

        if (!session->IsAuthenticated())
        {
            session->SendMessage(&response, sizeof(response));
            return;
        }

        //std::cout << "IsAuthenticated" << std::endl;

        std::string roomName(request.roomName);
        auto room = room_manager_.GetRoomByName(roomName);
        if (!room)
        {
            session->SendMessage(&response, sizeof(response));
            return;
        }

        //std::cout << "GetRoomByName" << std::endl;

        auto user = user_manager_.GetUser(session->GetUserId());
        if (!user)
        {
            session->SendMessage(&response, sizeof(response));
            return;
        }

        //std::cout << "GetUser" << std::endl;

        if (room->AddUser(user))
        {
            response.isSuccess = true;

            // 방 정보 알림 (옵션)
            PKT_NOTICE_ROOM_INFO notice{};
            notice.pkt_id = NOTICE_ROOM_INFO;
            notice.pkt_size = sizeof(PKT_NOTICE_ROOM_INFO);
            std::snprintf(notice.roomName, MAX_ROOM_NAME_LEN, "%s", roomName.c_str());
            notice.playerCount = static_cast<std::uint16_t>(room->GetUserCount());

            room->BroadcastMessage(NOTICE_ROOM_INFO, &notice, sizeof(notice));
        }

        session->SendMessage(&response, sizeof(response));
    }

    void ProcessLeaveRoom(std::shared_ptr<Session> session, const PKT_REQ_LEAVE_ROOM& request)
    {
        PKT_RES_LEAVE_ROOM response{};
        response.pkt_id = RES_LEAVE_ROOM;
        response.pkt_size = sizeof(PKT_RES_LEAVE_ROOM);
        response.isSuccess = false;

        if (!session->IsAuthenticated())
        {
            session->SendMessage(&response, sizeof(response));
            return;
        }

        std::string roomName(request.roomName);
        auto room = room_manager_.GetRoomByName(roomName);
        if (!room)
        {
            session->SendMessage(&response, sizeof(response));
            return;
        }

        uint32_t user_id = session->GetUserId();
        if (room->RemoveUser(user_id))
        {
            response.isSuccess = true;

            PKT_NOTICE_ROOM_INFO notice{};
            notice.pkt_id = NOTICE_ROOM_INFO;
            notice.pkt_size = sizeof(PKT_NOTICE_ROOM_INFO);
            std::snprintf(notice.roomName, MAX_ROOM_NAME_LEN, "%s", roomName.c_str());
            notice.playerCount = static_cast<std::uint16_t>(room->GetUserCount());

            room->BroadcastMessage(NOTICE_ROOM_INFO, &notice, sizeof(notice));
        }

        session->SendMessage(&response, sizeof(response));
    }

    void ProcessRoomListRequest(std::shared_ptr<Session> session, const PKT_REQ_ROOM_LIST& /*request*/)
    {
        PKT_RES_ROOM_LIST response{};
        response.pkt_id = RES_ROOM_LIST;
        response.pkt_size = sizeof(PKT_RES_ROOM_LIST);

        auto rooms = room_manager_.GetRoomList();
        std::uint16_t count = 0;

        for (auto& room : rooms)
        {
            if (count >= 10)
                break;

            std::snprintf(response.rooms[count].roomName, MAX_ROOM_NAME_LEN, "%s", room->GetName().c_str());
            response.rooms[count].playerCount =
                static_cast<std::uint16_t>(room->GetUserCount());
            ++count;
        }

        response.roomCount = count;
        session->SendMessage(&response, sizeof(response));
    }

    void ProcessPlayerState(std::shared_ptr<Session> session, const PKT_REQ_PLAYER_STATE& request)
    {
        if (!session->IsAuthenticated())
            return;

        auto user = user_manager_.GetUser(session->GetUserId());
        if (!user)
            return;

        user->SetCharacterState(request.characterState);
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
        packet_dispatcher_.RegisterHandler(REQ_LOGIN, std::make_unique<LoginHandler>(*this));
        packet_dispatcher_.RegisterHandler(REQ_LOGOUT, std::make_unique<LogoutHandler>(*this));
        packet_dispatcher_.RegisterHandler(REQ_CHAT, std::make_unique<ChatMessageHandler>(*this));
        packet_dispatcher_.RegisterHandler(REQ_CREATE_ROOM, std::make_unique<CreateRoomHandler>(*this));
        packet_dispatcher_.RegisterHandler(REQ_ENTER_ROOM, std::make_unique<EnterRoomHandler>(*this));
        packet_dispatcher_.RegisterHandler(REQ_LEAVE_ROOM, std::make_unique<LeaveRoomHandler>(*this));
        packet_dispatcher_.RegisterHandler(REQ_ROOM_LIST, std::make_unique<RoomListHandler>(*this));
        packet_dispatcher_.RegisterHandler(REQ_PLAYER_STATE, std::make_unique<PlayerStateHandler>(*this));
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;

    SessionManager session_manager_;
    UserManager    user_manager_;
    RoomManager    room_manager_;
    PacketDispatcher packet_dispatcher_;
};
