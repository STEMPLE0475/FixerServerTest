#include <iostream>

#include <boost/asio.hpp>
#include <google/protobuf/stubs/common.h>

#include "GameServer.h"

int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    boost::asio::io_context io_context;

    GameServer gameServer(io_context);
    gameServer.Start();

    io_context.run();
    
    std::cout << "메인 서버를 종료하려면 키를 누르세요... " << std::endl;
    getchar();

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
