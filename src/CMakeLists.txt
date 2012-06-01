
add_executable(redirect redirect.c attach.c ptrace.c ptrace.h)

include_directories(${CMAKE_BINARY_DIR})

install(TARGETS redirect RUNTIME DESTINATION bin)

