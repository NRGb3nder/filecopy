cmake_minimum_required(VERSION 3.9)
project(filecopy C)

set(CMAKE_C_STANDARD 11)

add_executable(filecopy main.c utils.h utils.c)