#pragma once

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
    {
    }

    std::shared_ptr<Room> CreateRoom( const std::string& name, uint32_t max_users = 100)
    {
        uint32_t room_id = next_room_id_++;
        auto room = std::make_shared<Room>(io_context_, room_id, name, max_users);
        
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

private:
    boost::asio::io_context& io_context_;
    std::unordered_map<uint32_t, std::shared_ptr<Room>> rooms_;
    mutable std::mutex rooms_mutex_;
    std::atomic<uint32_t> next_room_id_;
    
};
