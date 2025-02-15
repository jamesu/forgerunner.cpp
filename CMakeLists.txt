cmake_minimum_required(VERSION 3.10)
project(ForgeRunner)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-invalid-offsetof)
endif()

file(GLOB_RECURSE PROTO_SRC_FILES "proto_src/*.cc")
file(GLOB_RECURSE SRC_FILES "src/*.cc")

# Find libcurl
find_package(CURL REQUIRED)

# Find Protobuf
find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)

# Include directories
include_directories(${CURL_INCLUDE_DIRS} ${Protobuf_INCLUDE_DIRS} . ./proto_src ./include)

# Source file
add_executable(forgerunner ${SRC_FILES} ${PROTO_SRC_FILES})
add_executable(tokentest test/tokentest.cpp)

# Link libraries
target_link_libraries(forgerunner ${CURL_LIBRARIES} ${Protobuf_LIBRARIES}
    absl::log_internal_message
    absl::log_internal_check_op
    absl::status)
