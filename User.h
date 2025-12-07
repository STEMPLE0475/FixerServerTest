#pragma once

#include <cstdint>
#include <string>
#include<memory>

#include "Session.h"

class User
{
public:
    User(uint32_t id, const std::string& username)
        : id_(id), username_(username), is_online_(false)
    {
    }

    uint32_t GetId() const { return id_; }
    const std::string& GetUsername() const { return username_; }
    bool IsOnline() const { return is_online_; }
    void SetOnline(bool online) { is_online_ = online; }

    void SetSession(std::shared_ptr<Session> session) { session_ = session; }
    std::weak_ptr<Session> GetSession() const { return session_; }
    
    const CharacterState& GetCharacterState() const { return character_state_; }
    void SetCharacterState(const CharacterState& state) { character_state_ = state; }
    void ResetCharacterState() { character_state_ = CharacterState{}; }

private:
    uint32_t id_;
    std::string username_;
    bool is_online_;
    std::weak_ptr<Session> session_;
    CharacterState character_state_{};
};
