cmake_minimum_required(VERSION 3.5)
project(franka_desk_client)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(CHECK_TIDY "Adds clang-tidy tests" OFF)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(Poco REQUIRED COMPONENTS Foundation XML JSON Net Util Crypto NetSSL)

add_executable(${PROJECT_NAME}
        src/franka_desk_client.cpp
)

target_include_directories(
    ${PROJECT_NAME}
        PRIVATE
        include
)

ament_target_dependencies(
    ${PROJECT_NAME}
        rclcpp
)

target_link_libraries(${PROJECT_NAME} Poco::Foundation Poco::XML Poco::JSON Poco::Util Poco::Net Poco::Crypto Poco::NetSSL)


install(TARGETS
${PROJECT_NAME}
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
