cmake_minimum_required(VERSION 3.16)

project(EECS482_P4)
set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED thread system)

add_library(libfs_client STATIC IMPORTED)
add_library(libfs_server STATIC IMPORTED)

if (APPLE)
    add_compile_options(-D_XOPEN_SOURCE)
    set_target_properties(libfs_client PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libfs_client_macos.o)
    set_target_properties(libfs_server PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libfs_server_macos.o)
else ()
    set_target_properties(libfs_client PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libfs_client.o)
    set_target_properties(libfs_server PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libfs_server.o)
endif ()

# File server build target (customize this with the source files for your file server)
add_executable(fs fs_server.cpp)
target_link_libraries(fs libfs_server ${CMAKE_DL_LIBS} Boost::thread)
add_custom_command(POST_BUILD TARGET fs COMMAND ${CMAKE_CURRENT_LIST_DIR}/autotag.sh push)

# Add a build target for each test program
add_executable(app test4.cpp test5.cpp)
target_link_libraries(app libfs_client)