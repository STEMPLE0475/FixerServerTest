#pragma once

#include <memory>
#include <mutex>
#include <unordered_set>
#include <iostream>

#include "Session.h"

class SessionManager
{
public:
    void AddSession(std::shared_ptr<Session> session)
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.insert(session);
        std::cout << "Session added. Total sessions: " << sessions_.size() << std::endl;
    }

    void RemoveSession(std::shared_ptr<Session> session)
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session);
        std::cout << "Session removed. Total sessions: " << sessions_.size() << std::endl;
    }

    std::shared_ptr<Session> FindSessionByUserId(uint32_t user_id)
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : sessions_)
        {
            if (session->GetUserId() == user_id)
                return session;
        }
        return nullptr;
    }

    void BroadcastToAll(const void* data, size_t size)
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : sessions_)
        {
            if (session->IsAuthenticated())
            {
                session->SendMessage(data, size);
            }
        }
    }

    size_t GetSessionCount() const
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        return sessions_.size();
    }

    void DisconnectAll()
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : sessions_)
        {
            session->Disconnect();
        }
        sessions_.clear();
    }

private:
    std::unordered_set<std::shared_ptr<Session>> sessions_;
    mutable std::mutex sessions_mutex_;
};
