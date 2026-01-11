#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <queue>
#include <unordered_map>

#include "User.h"
#include "Protocol/Packet.pb.h"

class Room : public std::enable_shared_from_this<Room>
{
public:
    Room(boost::asio::io_context& io_context, uint32_t id, const std::string& name, const std::string& password, bool isPvp, uint32_t max_users = 10)
        : id_(id)
        , name_(name)
        , max_users_(max_users)
        , password_(password)
        , isPvp_(isPvp)
        , io_context_(io_context)
        , tick_timer_(io_context)
    {
        tick_ = 0;
    }
    
    // Room Manage
    uint32_t GetId() const { return id_; }
    uint32_t GetTick() const { return tick_; }
    bool GetIsPvp() const { return isPvp_; }
    const std::string& GetName() const { return name_; }
    const std::string& GetPassword() const { return password_; }

    // User Manage
    size_t GetUserCount() const
    {
        std::lock_guard<std::mutex> lock(users_mutex_);
        return users_.size();
    }
    bool AddUser(std::shared_ptr<User> user);
    bool RemoveUser(uint32_t user_id);
    std::vector<std::shared_ptr<User>> GetUserList() const;

    // PVP
    void AddAttackRequest(uint32_t trigger_id, uint32_t target_id);
    void TryParry(uint32_t trigger_id);
    void ProcessParry();
    void ProcessExpiredAttack();

    // Broadcast Message
    void BroadcastChat(const std::string& sender_name, const std::string& message, std::uint32_t exclude_user_id);
    void BroadcastNotification(const std::string& notification, uint32_t exclude_user_id);
    void BroadcastPlayerStates();
    void BroadcastRoomInfo();
    //void BroadcastPlayerInteract(); 

    // Send Packet
    void SendPacketToAll(fixer::PacketId pkt_id, const google::protobuf::Message& msg);
    void SendPacketToAllExcept(uint32_t exclude_user_id, fixer::PacketId pkt_id, const google::protobuf::Message& msg);
    void SendPacketToUser(uint32_t user_id, fixer::PacketId pkt_id, const google::protobuf::Message& msg);
    void SendPacketToUsers(const std::vector<uint32_t>& user_ids, fixer::PacketId pkt_id, const google::protobuf::Message& msg);

    // Tick & Timer
    void StartTick();
    void Update();
    void ScheduleNextTick();

private:
    // Room Manage
    uint32_t id_;
    std::string name_;
    std::string password_;
    uint32_t max_users_;
    bool isPvp_;

    // User Manage
    std::unordered_map<uint32_t, std::shared_ptr<User>> users_;
    mutable std::mutex users_mutex_;

    // PVP
    std::unordered_map<uint32_t, AttackRequest> attackRequestMap;
    std::vector<AttackRequest> parrySuccessList;
    std::priority_queue<std::pair<uint32_t, uint32_t>,
        std::vector<std::pair<uint32_t, uint32_t>>,
        std::greater<std::pair<uint32_t, uint32_t>>> attackRequestQueueByExpireTick;; // (expire_tick, target_user_id)
    mutable std::mutex interact_mutex_;

    // Send Packet Utility
    std::vector<char> BuildPacket(fixer::PacketId pkt_id, const google::protobuf::Message& msg);
    void SendPacketToSessions(const std::vector<std::shared_ptr<Session>>& sessions, const std::vector<char>& packet);

    // Tick & Timer
    uint32_t tick_;
    boost::asio::io_context& io_context_;
    boost::asio::steady_timer tick_timer_;
};
