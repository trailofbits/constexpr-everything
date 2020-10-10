cmake_minimum_required (VERSION 3.8)
project (constexpr-everything CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wall -fno-strict-aliasing -fno-exceptions -fno-rtti")

# NOTE(ww): When multiple versions of libclang are installed, CMake gets confused
# and picks the wrong (usually old) one. These settings tell it to assume a natural
# decreasing search order, which should fix *some* cases.
# See: https://github.com/CastXML/CastXML/issues/148 and https://github.com/mesonbuild/meson/issues/5636
set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)

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
        clangSerialization
        clangSema
        clangAnalysis
        clangFrontendTool
        clangDynamicASTMatchers
        clangRewrite
        clangRewriteFrontend
        clangToolingCore
        )

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

if((${LLVM_PACKAGE_VERSION} VERSION_LESS "9.0.0")
  AND (${LLVM_PACKAGE_VERSION} VERSION_GREATER_EQUAL  "11"))
  message(FATAL_ERROR "Only LLVM 9 and 10 are supported.")
endif()

# The clang package doesn't appear to provide a version
find_package(Clang REQUIRED CONFIG)
message(STATUS "Using ClangConfig.cmake in: ${CLANG_CMAKE_DIR}")

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
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
