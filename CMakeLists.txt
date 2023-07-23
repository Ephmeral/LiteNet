cmake_minimum_required(VERSION 3.22)
project(LiteNet)

# 设置调试信息，启动C++11标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 动态库最终存放的位置
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
# 设置test文件夹
set(LITENET_TEST_DIR ${PROJECT_SOURCE_DIR}/test)

include_directories(${LITENET_TEST_DIR})

add_subdirectory(test)

# 定义参与编译的源文件
aux_source_directory(. SRC_LIST)
# 编译生成动态库LiteNet
add_library(LiteNet SHARED ${SRC_LIST})
