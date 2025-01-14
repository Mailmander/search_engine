cmake_minimum_required(VERSION 3.22)
project(Cursach)

# Установіть стандарт C++
set(CMAKE_CXX_STANDARD 23)

# Додаємо виконуваний файл
add_executable(Cursach
        main.cpp
        InvertedIndex.cpp
        ThreadPool.cpp
        HTTPServer.cpp
        HTTPServer.h
        FileManager.cpp
        FileManager.h
)

# Лінкуємо Winsock для Windows
target_link_libraries(Cursach ws2_32)
