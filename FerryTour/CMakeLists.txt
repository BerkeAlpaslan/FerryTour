cmake_minimum_required(VERSION 3.10)
project(FerryTour C)

set(CMAKE_C_STANDARD 11)

# pthread kütüphanesini bul
find_package(Threads REQUIRED)

# Kaynak dosyaları ekle
add_executable(FerryTour main.c ferry.c)

# pthread kütüphanesini bağla
target_link_libraries(FerryTour ${CMAKE_THREAD_LIBS_INIT})