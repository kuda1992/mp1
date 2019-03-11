cmake_minimum_required(VERSION 3.13)
project(mp1)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(mp1
        testcases/msgdropsinglefailure.conf
        testcases/multifailure.conf
        testcases/singlefailure.conf
        Application.cpp
        Application.h
        dbg.0.log
        dbg.1.log
        dbg.2.log
        EmulNet.cpp
        EmulNet.h
        Grader.sh
        Log.cpp
        Log.h
        Makefile
        Member.cpp
        Member.h
        mp1_specifications.pdf
        MP1Node.cpp
        MP1Node.h
        Params.cpp
        Params.h
        Queue.h
        run.sh
        stdincludes.h
        submit.py)
