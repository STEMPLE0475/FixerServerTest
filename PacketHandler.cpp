#include "PacketHandler.h"
#include "Session.h"
#include "GameServer.h"

namespace
{
    template <typename TProto>
    void SendProtoResponse(std::shared_ptr<Session> session,
        fixer::PacketId pkt_id,
        const TProto& message)
    {
        std::string body;
        if (!message.SerializeToString(&body))
        {
            std::cout << "SerializeToString failed. pkt_id="
                << static_cast<int>(pkt_id) << std::endl;
            return;
        }

        std::uint16_t pkt_size =
            static_cast<std::uint16_t>(sizeof(PACKET_HEADER) + body.size());

        if (pkt_size > MAX_RECEIVE_BUFFER_LEN)
        {
            std::cout << "SendProtoResponse size too big: " << pkt_size << std::endl;
            return;
        }

        std::vector<char> buffer(pkt_size);

        auto* header = reinterpret_cast<PACKET_HEADER*>(buffer.data());
        header->pkt_id = static_cast<std::uint16_t>(pkt_id);
        header->pkt_size = pkt_size;

        if (!body.empty())
        {
            std::memcpy(buffer.data() + sizeof(PACKET_HEADER),
                body.data(), body.size());
        }

        session->SendMessage(buffer.data(), buffer.size());
    }
}

void LoginHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* body, std::size_t body_size)
{
    fixer::ReqLogin req;
    if (!req.ParseFromArray(body, static_cast<int>(body_size)))
        return;

    fixer::ResLogin res;
    res.set_user_id(0);
    res.set_is_success(false);

    const std::string& user_id = req.user_id();
    const std::string& password = req.password();

    if (!user_id.empty() && !password.empty())
    {
        auto user = users_.CreateUser(user_id);
        if (user)
        {
            user->SetOnline(true);
            user->SetSession(session);

            session->SetUserId(user->GetId());
            session->SetAuthenticated(true);

            res.set_user_id(user->GetId());
            res.set_is_success(true);

            std::cout << "User login: " << user->GetUsername()
                << " (ID: " << user->GetId() << ")" << std::endl;
        }
    }

    SendProtoResponse(session, fixer::PacketId::RES_LOGIN, res);
}

void GuestLoginHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* body, std::size_t body_size)
{
    fixer::ReqGuestLogin req;
    if (!req.ParseFromArray(body, static_cast<int>(body_size)))
        return;

    fixer::ResLogin res;
    res.set_user_id(0);
    res.set_is_success(false);

    const std::string& nickname = req.nickname();

    if (!nickname.empty())
    {
        auto user = users_.CreateUser(nickname);
        if (user)
        {
            user->SetOnline(true);
            user->SetSession(session);

            session->SetUserId(user->GetId());
            session->SetAuthenticated(true);

            res.set_user_id(user->GetId());
            res.set_is_success(true);

            std::cout << "User login: " << user->GetUsername()
                << " (ID: " << user->GetId() << ")" << std::endl;
        }
    }

    SendProtoResponse(session, fixer::PacketId::RES_LOGIN, res);
}

void LogoutHandler::HandlePacket(std::shared_ptr<Session> session, 
    const char* body, std::size_t body_size)
{
    fixer::ReqLogout req;
    if (body && body_size > 0)
    {
        // 필드가 있어도 상관 없이 Parse 시도
        if (!req.ParseFromArray(body, static_cast<int>(body_size)))
            return;
    }

    fixer::ResLogout res;
    res.set_is_success(false);

    if (!session->IsAuthenticated())
    {
        SendProtoResponse(session, fixer::PacketId::RES_LOGOUT, res);
        return;
    }

    uint32_t user_id = session->GetUserId();
    auto user = users_.GetUser(user_id);
    if (!user)
    {
        SendProtoResponse(session, fixer::PacketId::RES_LOGOUT, res);
        return;
    }

    // 방에 들어가 있으면 방에서 제거
    if (auto room = rooms_.FindRoomByUserId(user_id))
    {
        room->RemoveUser(user_id);
        room->BroadcastRoomInfo();
    }

    user->SetOnline(false);
    //user->ResetCharacterState();

    session->SetAuthenticated(false);
    session->SetUserId(0);

    res.set_is_success(true);

    SendProtoResponse(session, fixer::PacketId::RES_LOGOUT, res);

    // 로그아웃 후 세션을 끊고 싶다면 여기서 Close:
    // session->Close();
}

void ChatMessageHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* body, std::size_t body_size)
{
    if (!session->IsAuthenticated())
        return;

    fixer::ReqChat req;
    if (!req.ParseFromArray(body, static_cast<int>(body_size)))
        return;

    uint32_t user_id = session->GetUserId();

    auto user = users_.GetUser(user_id);
    if (!user)
        return;

    auto room = rooms_.FindRoomByUserId(user_id);
    if (!room)
        return;

    // Room이 NoticeChat 생성 + 브로드캐스트 담당
    room->BroadcastChat(user->GetUsername(), req.message(), 0);
}

void CreateRoomHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* body, std::size_t body_size)
{
    fixer::ReqCreateRoom req;
    if (!req.ParseFromArray(body, static_cast<int>(body_size)))
        return;

    fixer::ResCreateRoom res;
    res.set_is_success(false);

    if (!session->IsAuthenticated())
    {
        SendProtoResponse(session, fixer::PacketId::RES_CREATE_ROOM, res);
        return;
    }

    const std::string& room_name = req.room_name(); 

    if (room_name.empty())
    {
        SendProtoResponse(session, fixer::PacketId::RES_CREATE_ROOM, res);
        return;
    }

    uint32_t user_id = session->GetUserId();
    auto user = users_.GetUser(user_id);
    if (!user)
    {
        SendProtoResponse(session, fixer::PacketId::RES_CREATE_ROOM, res);
        return;
    }

    if (rooms_.FindRoomByUserId(user_id))
    {
        SendProtoResponse(session, fixer::PacketId::RES_CREATE_ROOM, res);
        return;
    }

    if (rooms_.GetRoomByName(room_name))
    {
        SendProtoResponse(session, fixer::PacketId::RES_CREATE_ROOM, res);
        return;
    }

    const std::string& pwd = req.room_password();
    //pwd 검사 로직 필요

    bool isPvp = req.is_pvp();

    auto room = rooms_.CreateRoom(room_name, pwd, isPvp);
    if (!room)
    {
        SendProtoResponse(session, fixer::PacketId::RES_CREATE_ROOM, res);
        return;
    }

    if (!room->AddUser(user))
    {
        SendProtoResponse(session, fixer::PacketId::RES_CREATE_ROOM, res);
        return;
    }

    
    room->BroadcastRoomInfo();

    res.set_is_success(true);
    res.set_room_id(room->GetId());
    SendProtoResponse(session, fixer::PacketId::RES_CREATE_ROOM, res);
}

void LeaveRoomHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* body, std::size_t body_size)
{
    fixer::ReqLeaveRoom req;
    if (body && body_size > 0)
    {
        // room_id 같은 필드가 있다면 Parse
        if (!req.ParseFromArray(body, static_cast<int>(body_size)))
            return;
    }

    fixer::ResLeaveRoom res;
    res.set_is_success(false);

    if (!session->IsAuthenticated())
    {
        SendProtoResponse(session, fixer::PacketId::RES_LEAVE_ROOM, res);
        return;
    }

    uint32_t user_id = session->GetUserId();
    auto user = users_.GetUser(user_id);
    if (!user)
    {
        SendProtoResponse(session, fixer::PacketId::RES_LEAVE_ROOM, res);
        return;
    }

    auto room = rooms_.FindRoomByUserId(user_id);
    if (!room)
    {
        SendProtoResponse(session, fixer::PacketId::RES_LEAVE_ROOM, res);
        return;
    }

    if (!room->RemoveUser(user_id))
    {
        SendProtoResponse(session, fixer::PacketId::RES_LEAVE_ROOM, res);
        return;
    }

    room->BroadcastRoomInfo();   // 인원 변경 Notice

    res.set_is_success(true);
    SendProtoResponse(session, fixer::PacketId::RES_LEAVE_ROOM, res);
}

void EnterRoomHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* body, std::size_t body_size)
{
    fixer::ReqEnterRoom req;
    if (!req.ParseFromArray(body, static_cast<int>(body_size)))
        return;

    fixer::ResEnterRoom res;
    res.set_is_success(false);

    if (!session->IsAuthenticated())
    {
        SendProtoResponse(session, fixer::PacketId::RES_ENTER_ROOM, res);
        return;
    }

    uint32_t room_id = req.room_id();
    if (room_id == 0)
    {
        // 방 번호가 0인 경우 에러
        SendProtoResponse(session, fixer::PacketId::RES_ENTER_ROOM, res);
        return;
    }

    auto room = rooms_.GetRoom(room_id);
    if (!room)
    {
        SendProtoResponse(session, fixer::PacketId::RES_ENTER_ROOM, res);
        return;
    }

    if (room->GetUserCount() >= 10) {
        //수용한도
        SendProtoResponse(session, fixer::PacketId::RES_ENTER_ROOM, res);
        return;
    }

    if (req.password() != room->GetPassword())
    {
        //비밀번호 오류
        SendProtoResponse(session, fixer::PacketId::RES_ENTER_ROOM, res);
        return;
    }
    
    uint32_t user_id = session->GetUserId();
    auto user = users_.GetUser(user_id);
    if (!user)
    {
        SendProtoResponse(session, fixer::PacketId::RES_ENTER_ROOM, res);
        // 플레이어를 찾을 수 없음
        return;
    }

    if (rooms_.FindRoomByUserId(user_id))
    {
        SendProtoResponse(session, fixer::PacketId::RES_ENTER_ROOM, res);
        // 플레이어가 이미 방에 있음
        return;
    }

    if (room->AddUser(user))
    {
        res.set_is_success(true);
        room->BroadcastRoomInfo();
    }

    SendProtoResponse(session, fixer::PacketId::RES_ENTER_ROOM, res);
}

void RoomListHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* body, std::size_t body_size)
{
    fixer::ReqRoomList req;
    if (body && body_size > 0)
    {
        if (!req.ParseFromArray(body, static_cast<int>(body_size)))
            return;
    }

    fixer::ResRoomList res;

    auto rooms = rooms_.GetRoomList();
    for (auto& room : rooms)
    {
        auto* entry = res.add_rooms();
        entry->set_room_name(room->GetName());
        entry->set_room_id(room->GetId());
        entry->set_player_count(static_cast<uint32_t>(room->GetUserCount()));
    }

    std::cout << res.rooms_size() << std::endl;
    SendProtoResponse(session, fixer::PacketId::RES_ROOM_LIST, res);
}

void PlayerStateHandler::HandlePacket(std::shared_ptr<Session> session,
    const char* body, std::size_t body_size)
{
    if (!session->IsAuthenticated())
        return;

    fixer::ReqPlayerState req;
    if (!req.ParseFromArray(body, static_cast<int>(body_size)))
        return;

    uint32_t user_id = session->GetUserId();
    auto user = users_.GetUser(user_id);
    if (!user)
        return;

    auto room = rooms_.FindRoomByUserId(user_id);
    if (!room)
        return;

    user->UpdateCharacterState(req.state()); 
}

/*
fixer::CharacterState last_character_state_{};
    fixer::CharacterState cur_character_state_{};
    bool isStateChanged = false;
*/
