cmake_minimum_required(VERSION 3.16)

project(FixerServer LANGUAGES CXX)

# 1) C++ 표준 / 빌드 타입
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 기본 빌드 타입이 없으면 Release로
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 2) 경고 좀 빡세게
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# 3) Boost.Asio (Linux/AWS 쪽)
#   - AWS EC2에서: sudo apt-get install libboost-all-dev (우분투 기준)
find_package(Boost 1.70 REQUIRED COMPONENTS system)

# 4) 소스 파일들
set(SOURCES
    FixerMVP.cpp   
    Session.cpp    
    PacketHandler.cpp  
    Room.cpp  
    # 필요하다면 여기다가 추가 cpp들 계속 나열
)

add_executable(FixerServer ${SOURCES})

# 5) 헤더 경로
target_include_directories(FixerServer
    PRIVATE
        ${Boost_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}   # Protocol.h, GameServer.h, Room.h 등 
)

# 6) 라이브러리 링크
target_link_libraries(FixerServer
    PRIVATE
        Boost::system
        pthread             # 리눅스에서 쓰레드
)

# 7) 리눅스용 최적화(선택)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(FixerServer PRIVATE _LINUX)
    # 필요하면 여기서 -O3 같은 최적화, LTO 옵션 추가 가능
endif()
