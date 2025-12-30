#include "Room.h"
#include <cstdio>
#include <iostream>

void Room::StartTick()
{
    ScheduleNextTick();
}

bool Room::AddUser(std::shared_ptr<User> user)
{
    size_t userCount = 0;
    {
        std::lock_guard<std::mutex> lock(users_mutex_);

        if (users_.size() >= max_users_)
            return false;

        if (users_.find(user->GetId()) != users_.end())
            return false; // 이미 방에 있음

        users_[user->GetId()] = user;
        for (auto& [id, u] : users_) {
            u->isStateChanged = true; // 새 유저를 위해 모든 유저의 상태를 '변경됨'으로 표시
        }
        userCount = users_.size();
    }

    user->ResetCharacterState();

    std::string notification = user->GetUsername() + " joined the room.";
    BroadcastNotification(notification, user->GetId());

    std::cout << "User " << user->GetUsername()
        << " joined room " << name_
        << " (Users: " << userCount << ")" << std::endl;

    return true;
}

bool Room::RemoveUser(uint32_t user_id)
{
    size_t userCount = 0;
    std::string username;
    {
        std::lock_guard<std::mutex> lock(users_mutex_);

        auto it = users_.find(user_id);
        if (it == users_.end())
            return false;

        username = it->second->GetUsername();
        it->second->ResetCharacterState();
        users_.erase(it);
        userCount = users_.size();
    }

    std::string notification = username + " left the room.";
    BroadcastNotification(notification, user_id);
    std::cout << "User " << username
        << " left room " << name_
        << " (Users: " << userCount << ")" << std::endl;

    return true;
}

void Room::BroadcastChat(const std::string& sender_name, const std::string& message,  std::uint32_t exclude_user_id)
{
    fixer::NoticeChat msg;
    msg.set_sender_name(sender_name);
    msg.set_message(message);

    if (exclude_user_id == 0)
        SendPacketToAll(fixer::PacketId::NOTICE_CHAT, msg);
    else
        SendPacketToAllExcept(exclude_user_id, fixer::PacketId::NOTICE_CHAT, msg);
}

void Room::BroadcastNotification(const std::string& notification, uint32_t exclude_user_id)
{
    fixer::NoticeChat msg;
    msg.set_sender_name("SYSTEM");
    msg.set_message(notification);

    if (exclude_user_id == 0)
        SendPacketToAll(fixer::PacketId::NOTICE_CHAT, msg);
    else
        SendPacketToAllExcept(exclude_user_id, fixer::PacketId::NOTICE_CHAT, msg);
}

void Room::BroadcastPlayerStates()
{
    fixer::NoticePlayerState msg;
    bool has_changes = false;

    {
        std::lock_guard<std::mutex> lock(users_mutex_);

        for (auto& [user_id, user] : users_)
        {
            // 1. 상태가 변한 유저만 체크
            if (user->isStateChanged)
            {
                auto* entry = msg.add_players();
                entry->set_user_id(user_id);

                // 2. 현재 상태 복사 및 플래그 초기화
                const fixer::CharacterState& cs = user->GetCharacterState();
                *entry->mutable_state() = cs;

                user->isStateChanged = false; // 플래그 끄기
                has_changes = true;
            }
        }
    }

    // 3. 변한 플레이어가 한 명이라도 있을 때만 전송
    if (has_changes)
    {
        SendPacketToAll(fixer::PacketId::NOTICE_PLAYER_STATE, msg);
    }
}

void Room::BroadcastRoomInfo()
{
    fixer::NoticeRoomInfo msg;

    {
        std::lock_guard<std::mutex> lock(users_mutex_);

        for (auto& [user_id, user] : users_)
        {
            if (!user) continue;

            auto* p = msg.add_players();  
            p->set_user_id(user_id);
            p->set_user_name(user->GetUsername()); 
        }
    }
    std::cout << "NOtice room info" << std::endl;
    SendPacketToAll(fixer::PacketId::NOTICE_ROOM_INFO, msg);
}


void Room::SendPacketToAll(fixer::PacketId pkt_id, const google::protobuf::Message& msg)
{
    auto packet = BuildPacket(pkt_id, msg);
    if (packet.empty())
        return;

    std::vector<std::shared_ptr<Session>> targets;
    {
        std::lock_guard<std::mutex> lock(users_mutex_);
        targets.reserve(users_.size());

        for (auto& [user_id, user] : users_)
        {
            auto session = user->GetSession().lock();
            if (session && user->IsOnline())
                targets.push_back(session);
        }
    }

    SendPacketToSessions(targets, packet);
}

void Room::SendPacketToAllExcept(uint32_t exclude_user_id, fixer::PacketId pkt_id, const google::protobuf::Message& msg)
{
    auto packet = BuildPacket(pkt_id, msg);
    if (packet.empty())
        return;

    std::vector<std::shared_ptr<Session>> targets;
    {
        std::lock_guard<std::mutex> lock(users_mutex_);
        targets.reserve(users_.size());

        for (auto& [user_id, user] : users_)
        {
            if (user_id == exclude_user_id)
                continue;

            auto session = user->GetSession().lock();
            if (session && user->IsOnline())
                targets.push_back(session);
        }
    }

    SendPacketToSessions(targets, packet);
}

void Room::SendPacketToUser(uint32_t user_id, fixer::PacketId pkt_id, const google::protobuf::Message& msg)
{
    auto packet = BuildPacket(pkt_id, msg);
    if (packet.empty())
        return;

    std::shared_ptr<Session> target_session;
    {
        std::lock_guard<std::mutex> lock(users_mutex_);

        auto it = users_.find(user_id);
        if (it == users_.end())
            return;

        target_session = it->second->GetSession().lock();
        if (!target_session || !it->second->IsOnline())
            return;
    }

    target_session->SendMessage(packet.data(), packet.size());
}

void Room::SendPacketToUsers(const std::vector<uint32_t>& user_ids, fixer::PacketId pkt_id, const google::protobuf::Message& msg)
{
    auto packet = BuildPacket(pkt_id, msg);
    if (packet.empty())
        return;

    std::vector<std::shared_ptr<Session>> targets;
    {
        std::lock_guard<std::mutex> lock(users_mutex_);

        for (uint32_t uid : user_ids)
        {
            auto it = users_.find(uid);
            if (it == users_.end())
                continue;

            auto session = it->second->GetSession().lock();
            if (session && it->second->IsOnline())
                targets.push_back(session);
        }
    }

    if (!targets.empty())
        SendPacketToSessions(targets, packet);
}

std::vector<std::shared_ptr<User>> Room::GetUserList() const
{
    std::lock_guard<std::mutex> lock(users_mutex_);
    std::vector<std::shared_ptr<User>> user_list;

    for (const auto& pair : users_)
    {
        user_list.push_back(pair.second);
    }

    return user_list;
}

std::vector<char> Room::BuildPacket(fixer::PacketId pkt_id, const google::protobuf::Message& msg)
{
    std::string body;
    if (!msg.SerializeToString(&body))
    {
        std::cout << "BuildPacket SerializeToString failed, pkt_id="
            << static_cast<int>(pkt_id) << "\n";
        return {};
    }

    std::uint16_t pkt_size =
        static_cast<std::uint16_t>(sizeof(PACKET_HEADER) + body.size());

    std::vector<char> buffer(pkt_size);

    auto* header = reinterpret_cast<PACKET_HEADER*>(buffer.data());
    header->pkt_id = static_cast<std::uint16_t>(pkt_id);
    header->pkt_size = pkt_size;

    std::memcpy(buffer.data() + sizeof(PACKET_HEADER),
        body.data(), body.size());

    return buffer;
}

void Room::SendPacketToSessions(const std::vector<std::shared_ptr<Session>>& sessions, const std::vector<char>& packet)
{
    if (packet.empty())
        return;

    for (auto& s : sessions)
    {
        if (!s) continue;
        s->SendMessage(packet.data(), packet.size());
    }
}

void Room::ScheduleNextTick()
{
    auto self = shared_from_this();

    using namespace std::chrono_literals;
    tick_timer_.expires_after(50ms); // 수정 예정
    tick_timer_.async_wait(
        [self](const boost::system::error_code& ec) {
            if (ec) return;

            self->BroadcastPlayerStates();

            self->ScheduleNextTick();
        });
}