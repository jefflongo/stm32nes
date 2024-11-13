cmake_minimum_required(VERSION 3.21)

project(stm32nes C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
  add_compile_options($<$<CONFIG:Debug>:/Zi)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wfatal-errors)
  add_compile_options($<$<CONFIG:Debug>:-g3>)
endif()

add_executable(nes src/mappers/mapper0.c src/cartridge.c src/cpu.c src/memory.c
                   src/nes.c src/main.c)
target_include_directories(nes PRIVATE src/include)
target_compile_definitions(nes PRIVATE PRINTF_SUPPORTED=1)

add_executable(cpu_test src/mappers/mapper0.c src/cartridge.c src/cpu.c
                        src/nes.c src/memory.c src/test.c)
target_include_directories(cpu_test PRIVATE src/include)
target_compile_definitions(cpu_test PRIVATE PRINTF_SUPPORTED=1)

enable_testing()

add_test(NAME cpu_test COMMAND $<TARGET_FILE:cpu_test>)
