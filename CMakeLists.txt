cmake_minimum_required(VERSION 3.25)
project(CLoTH_Gossip C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(CLoTH_Gossip
        include/array.h
        include/cloth.h
        include/event.h
        include/gossip.h
        include/heap.h
        include/htlc.h
        include/list.h
        include/network.h
        include/payments.h
        include/routing.h
        include/utils.h
        src/array.c
        src/cloth.c
        src/event.c
        src/heap.c
        src/htlc.c
        src/list.c
        src/network.c
        src/payments.c
        src/routing.c
        src/utils.c)

find_package(GSL REQUIRED)
target_link_libraries(CLoTH_Gossip GSL::gsl GSL::gslcblas)