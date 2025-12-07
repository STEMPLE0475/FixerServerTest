#include <iostream>
#include <boost/asio.hpp>

#include "GameServer.h"

int main()
{
    boost::asio::io_context io_context;

    GameServer gameServer(io_context);
    gameServer.Start();

    io_context.run();

    std::cout << "메인 서버를 종료하려면 키를 누르세요... " << std::endl;
    getchar();
    return 0;
}
