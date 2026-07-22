cmake_minimum_required(VERSION 3.5)
project(ThimPOS CXX)

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
if (NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    # Do nothing
elseif (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(CMAKE_CXX_STANDARD 20)
elseif (HAS_ANY AND HAS_STRING_VIEW)
    set(CMAKE_CXX_STANDARD 17)
else ()
    set(CMAKE_CXX_STANDARD 14)
endif ()
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} main.cc)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE
                           -finput-charset=UTF-8
                           -fexec-charset=UTF-8)
endif ()

option(THIMPOS_IGNORE_KEY_CHECK
       "Build ThimPOS without the startup license-key check (development only)"
       OFF)

# ##############################################################################
# If you include the drogon source code locally in your project, use this method
# to add drogon 
# add_subdirectory(drogon) 
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following lines
# Drogon được discover từ prefix chuẩn của toolchain MSYS2 UCRT64.
# Không hardcode CMAKE_PREFIX_PATH/Drogon_DIR vào project.
find_package(Drogon CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)

# gem-copilot is consumed as a small library. Its standalone Qt applications
# are intentionally not added to the ThimPOS build.
find_package(CURL REQUIRED)
target_sources(${PROJECT_NAME} PRIVATE gem-copilot/src/gemini_engine.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE gem-copilot/include)
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)

if (THIMPOS_IGNORE_KEY_CHECK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE THIMPOS_IGNORE_KEY_CHECK=1)
    message(WARNING "ThimPOS license-key verification is DISABLED for this build")
else ()
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    target_sources(${PROJECT_NAME} PRIVATE license/KeyManagerClient.cc)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/license)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto CURL::libcurl)
    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE advapi32 crypt32)
    endif ()
    message(STATUS "ThimPOS license-key verification is enabled")
endif ()

# ##############################################################################

if (CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "c++17 or higher is required")
elseif (CMAKE_CXX_STANDARD LESS 20)
    message(STATUS "use c++17")
else ()
    message(STATUS "use c++20")
endif ()

aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(models MODEL_SRC)

drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
                    ${CMAKE_CURRENT_BINARY_DIR})
# use the following line to create views with namespaces.
# drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
#                     ${CMAKE_CURRENT_BINARY_DIR} TRUE)
# use the following line to create views with namespace CHANGE_ME prefixed
# and path namespaces.
# drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
#                     ${CMAKE_CURRENT_BINARY_DIR} TRUE CHANGE_ME)

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/models)
target_sources(${PROJECT_NAME}
               PRIVATE
               ${SRC_DIR}
               ${CTL_SRC}
               ${FILTER_SRC}
               ${PLUGIN_SRC}
               ${MODEL_SRC})
# ##############################################################################
# uncomment the following line for dynamically loading views 
# set_property(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS ON)

# ##############################################################################

add_subdirectory(test)
