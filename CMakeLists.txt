cmake_minimum_required (VERSION 3.8)
project (constexpr-everything CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wno-unused-parameter -Wall -Wextra -Wshadow -fno-strict-aliasing -fno-exceptions -fno-rtti")

set(LIBRARY_LIST
        clangASTMatchers
        clangTooling
        clangDriver
        clangAST
        clangLex
        clangParse
        clangBasic
        clangFrontend
        clangEdit
        clangAST
        clangSerialization
        clangSema
        clangAST
        clangAnalysis
        clangASTMatchers
        clangFrontendTool
        clangDynamicASTMatchers
        clangTooling
        clangAST
        clangASTMatchers
        clangBasic
        clangFrontend
        clangLex
        clangRewrite
        clangRewriteFrontend
        clangTooling
        clangToolingCore
        )
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
if(LLVM_BUILD_MAIN_SRC_DIR)
  include_directories(${LLVM_BUILD_MAIN_SRC_DIR}/tools/clang/include)
  include_directories(${LLVM_BUILD_BINARY_DIR}/tools/clang/include)
endif()
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_definitions(
-D__STDC_LIMIT_MACROS
-D__STDC_CONSTANT_MACROS
)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${LIBRARY_LIST})

target_link_libraries(${PROJECT_NAME}
  LLVMTransformUtils
  LLVMAnalysis
  LLVMTarget
  LLVMOption # Support
  LLVMObject # BitReader, Core, Support
  LLVMBitReader # Core, Support
  LLVMCore # Support
  LLVMSupport
)
