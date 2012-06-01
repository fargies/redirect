
cmake_minimum_required(VERSION 2.6)

project(redirect)

enable_testing()

include(CheckFunctionExists)
check_function_exists(getopt_long HAVE_GETOPT_LONG)

configure_file(config.h.in config.h)

add_subdirectory(src)

