cmake_minimum_required(VERSION 3.15)
project(bicubic_interpolation)

# 添加编译选项
SET(CMAKE_CXX_FLAGS "-std=c++17 -O2")

set(PROJECT_SOURCE_DIR ../)

# 生成文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# include 目录
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 添加可执行文件
add_executable(bicubic_interpolation
        src/bicubic.cpp
)

set(OpenCV_DIR "F:/liuchang/environments/OpenCV/4.5.5/opencv-4.5.5/build/install")
# 寻找 opencv 的库
find_package(OpenCV COMPONENTS core highgui imgproc imgcodecs)
# 添加 opencv 的头文件
include_directories(${OpenCV_INCLUDE_DIRS} )
# 链接 opencv 动态库路径
link_directories(${OpenCV_LIBRARY_DIRS} )


## 设置库文件
target_link_libraries(bicubic_interpolation ${OpenCV_LIBS})