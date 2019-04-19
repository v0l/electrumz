cmake_minimum_required(VERSION 3.14)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
  set(USE_VCPKG ON)
  message("-- Using VCPKG")
endif()

project(electrumz)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++17 HAVE_FLAG_STD_CXX17)
if(NOT HAVE_FLAG_STD_CXX17)
    message(FATAL_ERROR "Your compiler does not support C++17")
endif()

if(UNIX) 
	find_library(L_PTHREAD pthread)
endif()
find_library(L_LMDB lmdb)
find_library(L_LIBUV uv)
find_library(L_MBEDTLS mbedtls)
find_library(L_MBEDTLS_CRYPTO mbedcrypto)
find_library(L_MBEDTLS_X509 mbedx509)
find_library(L_ARGTABLE argtable2)

set(SOURCES
	src/electrumz.cxx 
	src/net/NetWorker.cxx 
	src/net/JsonRPCServer.cxx 
	src/util/Config.cxx
	src/electrum/Commands.cxx
	src/blockchain/TXODB.cxx
	
	src/blockchain/bitcoin/strencodings.cpp
	src/blockchain/bitcoin/transaction.cpp
	src/blockchain/bitcoin/uint256.cpp
	src/blockchain/bitcoin/block.cpp
	src/blockchain/bitcoin/script.cpp
	src/blockchain/bitcoin/cleanse.cpp
)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_executable(electrumz ${SOURCES})
target_include_directories(electrumz BEFORE PRIVATE include)

target_link_libraries(electrumz ${L_LMDB})
target_link_libraries(electrumz ${L_LIBUV})
target_link_libraries(electrumz ${L_MBEDTLS})
target_link_libraries(electrumz ${L_MBEDTLS_CRYPTO})
target_link_libraries(electrumz ${L_MBEDTLS_X509})
target_link_libraries(electrumz ${L_ARGTABLE})
if(UNIX)
	if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERISON VERSION_LESS 9)
		message("-- Adding stdc++fs support for GCC < 9")
		target_link_libraries(electrumz stdc++fs)
	endif()
	target_link_libraries(electrumz ${L_PTHREAD})
endif()