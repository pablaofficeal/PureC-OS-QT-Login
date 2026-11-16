cmake_minimum_required(VERSION 3.21)
project(qt-login LANGUAGES C CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

get_filename_component(PUREC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(PUREC_BIN "${PUREC_ROOT}/bin")
if(DEFINED ENV{BIN_DIR})
  set(PUREC_BIN "$ENV{BIN_DIR}")
endif()

add_executable(qt-login
    src/main.cpp
    src/session.cpp src/session.h
    ${PUREC_ROOT}/src/programs/accounts/accounts.c
    ${PUREC_ROOT}/src/programs/accounts/accounts_core.c)
target_include_directories(qt-login PRIVATE
    ${PUREC_ROOT}/src/programs/accounts
    ${PUREC_ROOT}/libxcrypt/include)
target_link_libraries(qt-login PRIVATE Qt6::Widgets Qt6::QPurecIntegrationPlugin ${PUREC_BIN}/lib/libpurecrypt.a)
