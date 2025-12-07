#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>

#include "User.h"
#include "Protocol.h"

class Room : public std::enable_shared_from_this<Room>
{
public:
    Room(boost::asio::io_context& io_context, uint32_t id, const std::string& name, uint32_t max_users = 100)
        : id_(id)
        , name_(name)
        , max_users_(max_users)
        , io_context_(io_context)
        , tick_timer_(io_context)
    {
    }
    void StartTick();

    uint32_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }

    size_t GetUserCount() const
    {
        std::lock_guard<std::mutex> lock(users_mutex_);
        return users_.size();
    }

    bool AddUser(std::shared_ptr<User> user);
    bool RemoveUser(uint32_t user_id);

    // data에는 이미 PACKET_HEADER가 들어 있다고 가정
    void BroadcastMessage(PACKET_ID pkt_id, const void* data, size_t size,
        uint32_t sender_id = 0);

    void BroadcastNotification(const std::string& notification,
        uint32_t exclude_user_id);

    void BroadcastPlayerStates();

    std::vector<std::shared_ptr<User>> GetUserList() const;

private:
    void ScheduleNextTick();

    uint32_t id_;
    std::string name_;
    uint32_t max_users_;
    std::unordered_map<uint32_t, std::shared_ptr<User>> users_;
    mutable std::mutex users_mutex_;

    boost::asio::io_context& io_context_;
    boost::asio::steady_timer tick_timer_;

};
