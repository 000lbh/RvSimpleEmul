# CMakeList.txt: RvSimpleEmul 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.19)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("RvSimpleEmul")

# 将源代码添加到此项目的可执行文件。
add_executable (RvSimpleEmul
    "main.cpp"
    "RvInst.h"
    "RvInst.cpp"
    "RvExcept.hpp"
    "RvCpu.h"
    "RvCpu.cpp"
    "RvMem.h"
    "RvMem.cpp"
)

include_directories("3rd" "3rd/elfio")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RvSimpleEmul PROPERTY CXX_STANDARD 20)
endif()

# 如有需要，请添加测试并安装目标。
enable_testing()
add_test(NAME testadd COMMAND "sh" "-c" "./${PROJECT_NAME} -R ../testcases/testadd | grep a0=0x2d")
add_test(NAME testbubble COMMAND "sh" "-c" "./${PROJECT_NAME} -R ../testcases/testbubble | grep a0=0x8")
add_test(NAME testmul COMMAND "sh" "-c" "./${PROJECT_NAME} -R ../testcases/testmul | grep a0=0x32")
add_test(NAME testrecur COMMAND "sh" "-c" "./${PROJECT_NAME} -R ../testcases/testrecur | grep a0=0x37")
add_test(NAME testret COMMAND "sh" "-c" "./${PROJECT_NAME} -R ../testcases/testret | grep a0=0xbeef")
add_test(NAME testarg1 COMMAND "sh" "-c" "./${PROJECT_NAME} -R --arguments=\"1024 2048\" ../testcases/testarg | grep a0=0xc00")
add_test(NAME testarg2 COMMAND "sh" "-c" "./${PROJECT_NAME} -R --arguments=\"114514 1919810\" ../testcases/testarg | grep a0=0x1f0a94")
add_test(NAME testgcd1 COMMAND "sh" "-c" "./${PROJECT_NAME} -R --arguments=\"13 19\" ../testcases/testgcd | grep a0=0x1")
add_test(NAME testgcd2 COMMAND "sh" "-c" "./${PROJECT_NAME} -R --arguments=\"24 1024\" ../testcases/testgcd | grep a0=0x8")
add_test(NAME testgcd3 COMMAND "sh" "-c" "./${PROJECT_NAME} -R --arguments=\"91 169\" ../testcases/testgcd | grep a0=0xd")
add_test(NAME testgcd4 COMMAND "sh" "-c" "./${PROJECT_NAME} -R --arguments=\"114514 1919810\" ../testcases/testgcd | grep a0=0x2")
