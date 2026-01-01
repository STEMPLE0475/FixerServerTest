#pragma once

#include <boost/asio/steady_timer.hpp>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <iostream>

#include "Room.h"

class RoomManager
{
public:
    RoomManager(boost::asio::io_context& io_context) 
        : next_room_id_(1)
        , io_context_(io_context) 
        , room_log_timer_(io_context)
    {
    }

    std::shared_ptr<Room> CreateRoom( const std::string& name, const std::string& password, bool isPvp, uint32_t max_users = 10)
    {
        uint32_t room_id = next_room_id_++;
        auto room = std::make_shared<Room>(io_context_, room_id, name, password, isPvp, max_users);
        
        room->StartTick();
 
        {
            std::lock_guard<std::mutex> lock(rooms_mutex_);
            rooms_[room_id] = room;
        }
        

        std::cout << "Room created: " << name << " (ID: " << room_id << ")" << std::endl;
        return room;
    }

    std::shared_ptr<Room> GetRoom(uint32_t room_id)
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        auto it = rooms_.find(room_id);
        return (it != rooms_.end()) ? it->second : nullptr;
    }

    std::shared_ptr<Room> GetRoomByName(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        for (const auto& pair : rooms_)
        {
            if (pair.second->GetName() == name)
                return pair.second;
        }
        return nullptr;
    }

    bool RemoveRoom(uint32_t room_id)
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        auto it = rooms_.find(room_id);
        if (it != rooms_.end())
        {
            std::cout << "Room removed: " << it->second->GetName()
                << " (ID: " << room_id << ")" << std::endl;
            rooms_.erase(it);
            return true;
        }
        return false;
    }

    std::vector<std::shared_ptr<Room>> GetRoomList()
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        std::vector<std::shared_ptr<Room>> room_list;

        for (const auto& pair : rooms_)
        {
            room_list.push_back(pair.second);
        }

        return room_list;
    }

    size_t GetRoomCount() const
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        return rooms_.size();
    }

    void CleanupEmptyRooms()
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);

        for (auto it = rooms_.begin(); it != rooms_.end();)
        {
            if (it->second->GetUserCount() == 0)
            {
                std::cout << "Removing empty room: " << it->second->GetName() << std::endl;
                it = rooms_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    std::shared_ptr<Room> FindRoomByUserId(uint32_t user_id) const
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);

        for (const auto& pair : rooms_)
        {
            const auto& room = pair.second;
            if (!room)
                continue;

            auto users = room->GetUserList();
            for (auto& u : users)
            {
                if (u && u->GetId() == user_id)
                    return room;
            }
        }

        return nullptr;
    }

    void StartRoomCountLogging(std::chrono::seconds interval = std::chrono::seconds(30))
    {
        log_interval_ = interval;
        ScheduleRoomCountLog();
    }

private:
    void ScheduleRoomCountLog()
    {
        room_log_timer_.expires_after(log_interval_);
        room_log_timer_.async_wait([this](const boost::system::error_code& ec)
            {
                if (ec) return;

                int total_users = 0;
                size_t current_room_count = 0;
                std::vector<uint32_t> rooms_to_remove;

                // 1. 최소한의 범위에서 정보만 수집 (락 범위)
                {
                    std::lock_guard<std::mutex> lock(rooms_mutex_);
                    for (const auto& [room_id, room] : rooms_)
                    {
                        int count = room->GetUserCount();
                        if (count > 0) total_users += count;
                        else rooms_to_remove.push_back(room_id);
                    }
                    // 로그용 개수는 여기서 미리 복사해둠 (락 해제 후 GetRoomCount 호출 방지)
                    current_room_count = rooms_.size();
                }

                // 2. 락이 풀린 상태에서 안전하게 삭제 (RemoveRoom 내부의 락과 충돌 안 함)
                for (uint32_t id : rooms_to_remove)
                {
                    if (RemoveRoom(id))
                    {
                        std::cout << "[RoomManager] EmptyRoomCleaned: " << id << std::endl;
                    }
                }

                // 3. 로그 출력 (GetRoomCount 대신 미리 복사한 변수 사용)
                std::cout << "[RoomManager] " << NowToString()
                    << " room_count : " << current_room_count
                    << " total_user_count : " << total_users << std::endl;

                ScheduleRoomCountLog();
            });
    }

    void CleanUpEmptyRoom(uint32_t room_id)
    {
        RemoveRoom(room_id);
        std::cout << "[RoomManager] EmptyRoomCleaned" << std::endl;
    }

    //현재 타임스탬프 유틸
    std::string NowToString()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        std::time_t tt = system_clock::to_time_t(now);

        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);   // Windows
#else
        localtime_r(&tt, &tm);  // Linux
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    boost::asio::io_context& io_context_;
    std::unordered_map<uint32_t, std::shared_ptr<Room>> rooms_;
    mutable std::mutex rooms_mutex_;
    std::atomic<uint32_t> next_room_id_;

    boost::asio::steady_timer room_log_timer_;
    std::chrono::minutes log_interval_{ 5 };
};
