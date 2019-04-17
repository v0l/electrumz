cmake_minimum_required(VERSION 3.14)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
  set(USE_VCPKG ON)
  message("-- Using VCPKG")
endif()

project(electrumz)

if(${USE_VCPKG})
    find_package(unofficial-libuv CONFIG REQUIRED)
else()
    find_package(libuv CONFIG REQUIRED)
endif()
find_package(lmdb CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

find_library(MBEDTLS_LIB "mbedtls")
find_library(MBEDCRYPTO_LIB "mbedcrypto")
find_library(MBEDX509_LIB "mbedx509")

add_subdirectory(proto)

set(SOURCES
	src/electrumz.cxx 
	src/net/NetWorker.cxx 
	src/net/JsonRPCServer.cxx 
	src/util/Config.cxx
	src/electrum/Commands.cxx
	src/blockchain/TXODB.cxx
	
	src/blockchain/btc/block.cxx
	src/blockchain/btc/tx.cxx
)

add_executable(electrumz ${SOURCES})
target_include_directories(electrumz BEFORE PRIVATE include)
set_property(TARGET electrumz PROPERTY CXX_STANDARD 17)

if(${USE_VCPKG})
    target_link_libraries(electrumz PRIVATE unofficial::libuv::libuv)
else()
    target_link_libraries(electrumz PRIVATE libuv)
endif()
target_link_libraries(electrumz PRIVATE lmdb)
target_link_libraries(electrumz PRIVATE spdlog::spdlog)
target_link_libraries(electrumz PRIVATE ${MBEDTLS_LIB})
target_link_libraries(electrumz PRIVATE ${MBEDCRYPTO_LIB})
target_link_libraries(electrumz PRIVATE ${MBEDX509_LIB})
target_link_libraries(electrumz PRIVATE proto)