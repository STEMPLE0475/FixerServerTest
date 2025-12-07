#include "Room.h"
#include "Protocol.h"
#include <cstring>
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

void Room::BroadcastMessage(PACKET_ID /*pkt_id*/, const void* data, size_t size,
    uint32_t sender_id)
{
    std::vector<std::shared_ptr<Session>> targets;

    {
        std::lock_guard<std::mutex> lock(users_mutex_);
        targets.reserve(users_.size());

        for (const auto& pair : users_)
        {
            if (sender_id != 0 && pair.first == sender_id) continue;

            auto session = pair.second->GetSession().lock();
            if (session && pair.second->IsOnline())
            {
                targets.push_back(session);
            }
        }
    }

    for (auto& session : targets)
    {
        session->SendMessage(data, size);
    }
}

void Room::BroadcastNotification(const std::string& notification,
    uint32_t exclude_user_id)
{
    // SYSTEM 채팅 패킷으로 브로드캐스트
    PKT_NOTICE_CHAT pkt{};
    pkt.pkt_id = NOTICE_CHAT;
    pkt.pkt_size = sizeof(PKT_NOTICE_CHAT);

    strncpy_s(pkt.senderName, MAX_NAME_LEN, "SYSTEM", _TRUNCATE);
    strncpy_s(pkt.message, MAX_MESSAGE_LEN, notification.c_str(), _TRUNCATE);

    std::lock_guard<std::mutex> lock(users_mutex_);
    for (const auto& pair : users_)
    {
        if (pair.first == exclude_user_id)
            continue;

        auto user = pair.second;
        auto session = user->GetSession().lock();
        if (session && user->IsOnline())
        {
            session->SendMessage(&pkt, sizeof(pkt));
        }
    }

}

void Room::BroadcastPlayerStates()
{
    PKT_NOTICE_PLAYER_STATE pkt{};
    pkt.pkt_id = NOTICE_PLAYER_STATE;
    pkt.count = 0;

    {
        std::lock_guard<std::mutex> lock(users_mutex_);

        for (const auto& [user_id, user] : users_)
        {
            if (pkt.count >= MAX_PLAYERS_PER_ROOM)
                break;

            auto& entry = pkt.players[pkt.count];
            entry.userId = user_id;
            entry.state = user->GetCharacterState();
            ++pkt.count;
            std::cout << "[ID]" << entry.userId << " [Pox]" << entry.state.pos_x << "," << entry.state.pos_y << std::endl;
        }
    }

    pkt.pkt_size = sizeof(PKT_NOTICE_PLAYER_STATE);

    BroadcastMessage(NOTICE_PLAYER_STATE, &pkt, sizeof(pkt));
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
