cmake_minimum_required(VERSION 3.16)
project(RiskMap LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(SDL2 CONFIG REQUIRED)
add_executable(risk-map main.cpp)
target_link_libraries(risk-map PRIVATE SDL2::SDL2)
add_custom_command(TARGET risk-map POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:risk-map>/assets)
enable_testing()
add_test(NAME map-interaction COMMAND risk-map --self-test)
set_tests_properties(map-interaction PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
