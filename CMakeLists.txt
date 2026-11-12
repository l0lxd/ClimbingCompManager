cmake_minimum_required(VERSION 3.20)
project(kletterwettkampf CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Crow: braucht Asio (standalone) ODER Boost. Du hast Boost: dann reicht
# `pacman -S boost` (CachyOS) bzw. libboost-all-dev (Debian/Ubuntu).
# Alternativ standalone Asio: `pacman -S asio` bzw. libasio-dev.
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.3.0)
  
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SQLiteCpp
  GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
  GIT_TAG 3.3.2)    # Tag bitte gegen die aktuelle Release-Seite pruefen

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include cstdint")
FetchContent_MakeAvailable(Crow SQLiteCpp)

add_executable(kletter src/main.cpp)
target_link_libraries(kletter PRIVATE Crow::Crow SQLiteCpp)
