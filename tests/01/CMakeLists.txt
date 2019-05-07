project(test01 CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

add_executable(${PROJECT_NAME} test01.cpp)
