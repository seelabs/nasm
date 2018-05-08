cmake_minimum_required(VERSION 3.1.0)

if (NOT CMAKE_C_COMPILER)
  find_program(CMAKE_C_COMPILER clang)
endif()

if (NOT CMAKE_CXX_COMPILER)
  find_program(CMAKE_CXX_COMPILER clang++)
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

project(codec_test)
enable_language(ASM_NASM)

function(prepend var prefix)
  set(listVar "")
  foreach(f ${ARGN})
    list(APPEND listVar "${prefix}${f}")
  endforeach(f)
  set(${var} "${listVar}" PARENT_SCOPE)
endfunction()

macro(append_flags name)
  foreach (arg ${ARGN})
    set(${name} "${${name}} ${arg}")
  endforeach()
endmacro()


set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(Boost_USE_STATIC_LIBS on)
set(Boost_USE_MULTITHREADED on)
set(Boost_USE_STATIC_RUNTIME off)

find_package(Boost REQUIRED
  COMPONENTS
  program_options
  )

prepend(srcs
  src/
  main.cpp
  )

prepend(asm_srcs
  src/
  decode.asm
  encode.asm
  decode58.asm
  )

add_executable(${PROJECT_NAME} ${srcs} ${asm_srcs})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)

target_link_libraries(${PROJECT_NAME}
  Boost::boost
  Boost::program_options
  )

target_include_directories(${PROJECT_NAME} PUBLIC src)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wall -ggdb -fno-omit-frame-pointer>)
append_flags(CMAKE_EXE_LINKER_FLAGS -ggdb)

