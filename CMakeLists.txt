cmake_minimum_required(VERSION 4.2)
project(CapstoneProject_ADVCS)

set(CMAKE_CXX_STANDARD 20)

add_executable(CapstoneProject_ADVCS main.cpp
        Models.h
        DriverRegistry.cpp
        DriverRegistry.h
        RatingEngine.cpp
        RatingEngine.h
        F1Parser.cpp
        F1Parser.h)
