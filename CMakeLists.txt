cmake_minimum_required(VERSION 3.15)

project(realm)

# Project-wide build flags
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(realm-core)

set(REALM_INCLUDE_CERTS 0)
set(REALM_BUILD_LIB_ONLY)
set(REALM_NO_CONFIG)
set(REALM_ENABLE_SYNC)

add_compile_definitions(REALM_NO_CONFIG)
add_compile_definitions(REALM_ENABLE_SYNC)

add_compile_definitions(REALM_VERSION_MAJOR=0)
add_compile_definitions(REALM_VERSION_MINOR=0)
add_compile_definitions(REALM_VERSION_PATCH=1)

set(SOURCES
    src/cpprealm/sdk.cpp
    src/cpprealm/object.cpp
) # REALM_SOURCES

set(HEADERS
    src/cpprealm/sdk.hpp
    src/cpprealm/app.hpp
    src/cpprealm/db.hpp
    src/cpprealm/object.hpp
    src/cpprealm/persisted.hpp
    src/cpprealm/results.hpp
    src/cpprealm/schema.hpp
    src/cpprealm/task.hpp
    src/cpprealm/thread_safe_reference.hpp
    src/cpprealm/type_info.hpp
) # REALM_INSTALL_HEADERS

add_library(cpprealm STATIC ${SOURCES} ${HEADERS})

target_include_directories(cpprealm PRIVATE realm-core/src)
target_include_directories(cpprealm PUBLIC /usr/local/include)
target_sources(cpprealm PRIVATE ${SOURCES})
target_link_libraries(cpprealm RealmCore ObjectStore ${CURL_LIBRARIES})
set_target_properties(cpprealm PROPERTIES PUBLIC_HEADER "${HEADERS}")

set(CURL_LIBRARY "-lcurl")
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
install(TARGETS cpprealm
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cpprealm
)
