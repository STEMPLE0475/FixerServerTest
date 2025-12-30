#pragma once

#include <cstdint>
#include <string>
#include<memory>

#include "Session.h"
#include "Protocol/Packet.pb.h"

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
    
    //Character State Snapshot
    const fixer::CharacterState& GetCharacterState() const { return cur_character_state_; }
    void SetCharacterState(const fixer::CharacterState& state) { cur_character_state_ = state; }
    void ResetCharacterState() { cur_character_state_ = fixer::CharacterState{}; }
    void UpdateCharacterState(const fixer::CharacterState& newState)
    {
        bool isMoved = std::abs(cur_character_state_.pos_x() - newState.pos_x()) > 0.01f ||
            std::abs(cur_character_state_.pos_y() - newState.pos_y()) > 0.01f;

        bool isStateUpdated = (cur_character_state_.facing_dir() != newState.facing_dir()) ||
            (cur_character_state_.action_state() != newState.action_state());

        if (isMoved || isStateUpdated)
        {
            cur_character_state_ = newState;
            isStateChanged = true;
        }
    }
    bool isStateChanged = false;

private:
    uint32_t id_;
    std::string username_;
    bool is_online_;
    std::weak_ptr<Session> session_;
    fixer::CharacterState cur_character_state_{};
    
};
