#include "Session.h"
#include "GameServer.h"
#include "Protocol.h"

#include <cstring>

using boost::asio::async_read;
using boost::asio::async_write;
using boost::asio::buffer;
using boost::system::error_code;

Session::Session(tcp::socket socket, GameServer& server)
    : socket_(std::move(socket))
    , server_(server)
{
}

Session::~Session() = default;

void Session::Start()
{
    DoReadHeader();
}

void Session::Disconnect()
{
    bool expected = false;
    if (!is_disconnected_.compare_exchange_strong(expected, true))
        return; // 이미 끊긴 상태

    error_code ec;
    try
    {
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
    catch (...)
    {
        // 무시
    }

    // GameServer에게 세션 종료 알림
    server_.OnSessionDisconnected(shared_from_this());
}

void Session::DoReadHeader()
{
    if (IsDisconnected())
        return;

    auto self = shared_from_this();
    async_read(
        socket_,
        buffer(&read_header_, sizeof(PACKET_HEADER)),
        [this, self](const error_code& ec, std::size_t bytes_transferred)
        {
            if (ec || bytes_transferred != sizeof(PACKET_HEADER))
            {
                Disconnect();
                return;
            }

            // 헤더 검증
            if (read_header_.pkt_size < sizeof(PACKET_HEADER) ||
                read_header_.pkt_size > MAX_RECEIVE_BUFFER_LEN)
            {
                std::cout << "Invalid packet size: "
                    << read_header_.pkt_size << std::endl;
                Disconnect();
                return;
            }

            const std::size_t body_size =
                static_cast<std::size_t>(read_header_.pkt_size) - sizeof(PACKET_HEADER);
            read_body_.resize(body_size);

            if (body_size == 0)
            {
                // 바디가 없는 패킷
                std::vector<char> packet(read_header_.pkt_size);
                std::memcpy(packet.data(), &read_header_, sizeof(PACKET_HEADER));
                ProcessPacket(packet.data(), packet.size());
                DoReadHeader();
            }
            else
            {
                DoReadBody(body_size);
            }
        });
}

void Session::DoReadBody(std::size_t body_size)
{
    if (IsDisconnected())
        return;

    auto self = shared_from_this();
    async_read(
        socket_,
        buffer(read_body_.data(), body_size),
        [this, self](const error_code& ec, std::size_t bytes_transferred)
        {
            if (ec || bytes_transferred != read_body_.size())
            {
                Disconnect();
                return;
            }

            // 헤더 + 바디를 하나의 패킷 버퍼로 합치기
            const std::size_t packet_size =
                sizeof(PACKET_HEADER) + read_body_.size();

            std::vector<char> packet(packet_size);
            std::memcpy(packet.data(), &read_header_, sizeof(PACKET_HEADER));
            if (!read_body_.empty())
            {
                std::memcpy(packet.data() + sizeof(PACKET_HEADER),
                    read_body_.data(),
                    read_body_.size());
            }

            ProcessPacket(packet.data(), packet.size());

            // 다음 패킷 읽기
            DoReadHeader();
        });
}

void Session::ProcessPacket(const char* data, std::size_t size)
{
    if (size < sizeof(PACKET_HEADER))
        return;

    const auto& header = *reinterpret_cast<const PACKET_HEADER*>(data);

    // packet header의 입력된 size에 맞게 잘 들어왔는지 검사
    if (header.pkt_size != size ||
        header.pkt_size > MAX_RECEIVE_BUFFER_LEN)
    {
        std::cout << "Packet size mismatch. header: "
            << header.pkt_size
            << ", actual: " << size << std::endl;
        return;
    }

    // GamerServer의 Dispatcher을 반환하여, 패킷에 대한 정보를 전달
    server_.GetPacketDispatcher().DispatchPacket(
        shared_from_this(), header, data, size);
}

void Session::SendMessage(const void* data, std::size_t size)
{
    if (IsDisconnected())
        return;

    if (size > MAX_RECEIVE_BUFFER_LEN)
    {
        std::cout << "SendMessage size too big: " << size << std::endl;
        return;
    }

    std::vector<char> buffer_copy(
        static_cast<const char*>(data),
        static_cast<const char*>(data) + size);

    bool should_start_write = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_queue_.emplace_back(std::move(buffer_copy));
        if (!write_in_progress_)
        {
            write_in_progress_ = true;
            should_start_write = true;
        }
    }

    if (should_start_write)
    {
        DoWrite();
    }
}

void Session::DoWrite()
{
    if (IsDisconnected())
        return;

    std::vector<char> packet_to_send;

    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.empty())
        {
            write_in_progress_ = false;
            return;
        }

        packet_to_send = write_queue_.front();
    }

    auto self = shared_from_this();
    async_write(
        socket_,
        buffer(packet_to_send.data(), packet_to_send.size()),
        [this, self](const error_code& ec, std::size_t /*bytes_transferred*/)
        {
            if (ec)
            {
                Disconnect();
                return;
            }

            bool has_more = false;
            {
                std::lock_guard<std::mutex> lock(write_mutex_);
                if (!write_queue_.empty())
                {
                    write_queue_.pop_front();
                }

                if (!write_queue_.empty())
                {
                    has_more = true;
                }
                else
                {
                    write_in_progress_ = false;
                }
            }

            if (has_more && !IsDisconnected())
            {
                DoWrite();
            }
        });
}
