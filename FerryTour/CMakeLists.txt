cmake_minimum_required(VERSION 3.31)
project(FerrySimulation C)

set(CMAKE_C_STANDARD 11)

add_executable(FerryTour
        main.c
        ferry.c
        ferry.h
)