#pragma once

#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <iostream>

#include <boost/asio.hpp>

#include "Protocol.h"

class GameServer; // 전방 선언

class Session : public std::enable_shared_from_this<Session>
{
public:
    using tcp = boost::asio::ip::tcp;

    Session(tcp::socket socket, GameServer& server);
    ~Session();

    // 세션 시작 (읽기 루프 시작)
    void Start();

    // 연결 종료 (중복 호출 방지)
    void Disconnect();

    // 패킷(메시지) 전송 – GameServer / Room에서 사용
    void SendMessage(const void* data, std::size_t size);

    // 소켓 직접 접근용 (GameServer에서 async_accept에 사용)
    tcp::socket& GetSocket() { return socket_; }
    const tcp::socket& GetSocket() const { return socket_; }

    // 로그인된 유저 ID
    uint32_t GetUserId() const { return user_id_; }
    void SetUserId(uint32_t id) { user_id_ = id; }

    // 인증 여부
    bool IsAuthenticated() const { return is_authenticated_; }
    void SetAuthenticated(bool v) { is_authenticated_ = v; }

    bool IsDisconnected() const { return is_disconnected_; }

private:
    void DoReadHeader();
    void DoReadBody(std::size_t body_size);
    void ProcessPacket(const char* data, std::size_t size);

    void DoWrite();

private:
    tcp::socket socket_;
    GameServer& server_;

    // 수신 버퍼
    PACKET_HEADER read_header_{};
    std::vector<char> read_body_;

    // 송신 큐
    std::mutex write_mutex_;
    std::deque<std::vector<char>> write_queue_;
    bool write_in_progress_ = false;

    // 세션 상태
    uint32_t user_id_ = 0;
    std::atomic<bool> is_authenticated_{ false };
    std::atomic<bool> is_disconnected_{ false };
};
