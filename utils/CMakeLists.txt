project(qaptbatch)
cmake_minimum_required(VERSION 2.6.2)

find_package(KDE4 REQUIRED)
include(FindPkgConfig REQUIRED)
include(KDE4Defaults)

# find_package(QApt REQUIRED)

add_subdirectory(qapt-batch)
