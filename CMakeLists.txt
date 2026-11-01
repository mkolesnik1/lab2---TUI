cmake_minimum_required(VERSION 3.20)
project(mailclient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MAIL_SANITIZE "Build with AddressSanitizer + UBSan" OFF)
option(MAIL_BUILD_TESTS "Build tests" ON)

add_library(mail_warnings INTERFACE)
target_compile_options(mail_warnings INTERFACE -Wall -Wextra -Werror)
if(MAIL_SANITIZE)
  target_compile_options(mail_warnings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(mail_warnings INTERFACE -fsanitize=address,undefined)
endif()

find_package(Threads REQUIRED)

# Слои (зависимости только вниз):
#   mail_core            = Core/Domain (+ Core/Application позже)   (без TUI, без SQLite)
#   mail_infrastructure  = реализации портов ядра (iconv, Maildir, SQLite)
add_subdirectory(src/core)

