cmake_minimum_required(VERSION 3.20)
project(abstracto_world LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glm CONFIG REQUIRED)

add_library(abstracto_world INTERFACE)
target_include_directories(abstracto_world INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(abstracto_world INTERFACE glm::glm)

if(BUILD_TESTING AND TARGET AbstractoWorldGridTest)
  target_link_libraries(AbstractoWorldGridTest PRIVATE abstracto_world)
  add_test(NAME AbstractoWorldGridTest COMMAND AbstractoWorldGridTest)
endif()
