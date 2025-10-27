cmake_minimum_required(VERSION 3.30)
project(ppmask)

if (NOT WIN32)
    message(FATAL_ERROR "This project is for Windows only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")

add_definitions(-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0600 -DUNICODE -D_UNICODE)

set(SOURCES
        main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

if (MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE Psapi)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE psapi)
endif()
