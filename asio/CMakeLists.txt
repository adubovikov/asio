# Minimum supported CMake version
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

if(CONAN_COMPILER)

  # ==========================================================
  project(asio VERSION ${CONAN_PACKAGE_VERSION} LANGUAGES CXX)
  # ==========================================================

  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  conan_basic_setup(TARGETS)    # this set CONAN_TARGETS

else()

  # use ccache and clang++ if found
  find_program(CCACHE_EXECUTABLE "ccache" HINTS /usr/local/bin /opt/local/bin)
  if(CCACHE_EXECUTABLE)
    message(STATUS "use ccache")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}" CACHE PATH "ccache" FORCE)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}" CACHE PATH "ccache" FORCE)
  endif()

  # ========================================
  project(asio VERSION 1.18.0 LANGUAGES CXX)
  # ========================================

endif()

#----------------------------------------------------------
# Compiler config
#----------------------------------------------------------
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()
message(STATUS "USING: CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}")
message(STATUS "USING: CMAKE_CXX_STANDARD_REQUIRED=${CMAKE_CXX_STANDARD_REQUIRED}")
message(STATUS "USING: CMAKE_CXX_EXTENSIONS=${CMAKE_CXX_EXTENSIONS}")


###########################################################
option(ASIO_STANDALONE "Build asio without boost" ON)
###########################################################
if(ASIO_STANDALONE)
  option(COMPILER_WARNINGS_ARE_ERRORS "To be pedantic! ;-)" ON)
  if(COMPILER_WARNINGS_ARE_ERRORS)
    if(MSVC)
      # warning level 4 and all warnings as errors
      add_compile_options(/W4 /WX)
    else()
      # lots of warnings and all warnings as errors
      add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    endif()
  endif()
else()
  unset(COMPILER_WARNINGS_ARE_ERRORS CACHE)
endif()
###########################################################


# =========================================================
# ASIO Options
#==========================================================
option(ASIO_NO_DEPRECATED "Build asio without deprecated interfaces" ON)
option(ASIO_BUILD_TESTS "Build asio unittest too" OFF)
option(ASIO_BUILD_EXAMPLES "Build asio examples too" OFF)
option(ASIO_ENABLE_HANDLER_TRACKING "enable handler tracking" OFF)
option(ASIO_DISABLE_KQUEUE "disable kqueue" OFF)
option(ASIO_DISABLE_EPOLL "disable epoll" OFF)
option(ASIO_USE_CLANG_TIDY "use clang-tidy if found" OFF)


#
# Where to put all the RUNTIME targets when built.  This variable is used
# to initialize the RUNTIME_OUTPUT_DIRECTORY property on all the targets.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)


set(CPPdefinitions)
set(EXTRA_LIBS)

if(NOT CONAN_COMPILER AND WIN32)
  list(APPEND CPPdefinitions _WIN32_WINNT=0x0501 __USE_W32_SOCKETS
    ASIO_HAS_VARIADIC_TEMPLATES ASIO_DISABLE_CONSTEXPR
  )
  set(EXTRA_LIBS wsock32 ws2_32)
else()
  set(EXTRA_LIBS ${CONAN_TARGETS})
endif()

if(NOT CONAN_COMPILER AND ${CMAKE_HOST_SYSTEM_NAME} MATCHES "MINGW.*")
  list(APPEND CPPdefinitions _WIN32_WINNT=0x0501 __USE_W32_SOCKETS)
endif()


if(ASIO_NO_DEPRECATED)
  message(STATUS "disable deprected interfaces")
  list(APPEND CPPdefinitions ASIO_NO_DEPRECATED)
endif()

if(ASIO_ENABLE_HANDLER_TRACKING)
  message(STATUS "enable handler tracking")
  list(APPEND CPPdefinitions ASIO_ENABLE_HANDLER_TRACKING)
endif()

if(ASIO_DISABLE_KQUEUE)
  message(STATUS "disble queue")
  list(APPEND CPPdefinitions ASIO_DISABLE_KQUEUE)
endif()

if(ASIO_DISABLE_EPOLL)
  message(STATUS "disable epoll")
  list(APPEND CPPdefinitions ASIO_DISABLE_EPOLL)
endif()

if(ASIO_USE_CLANG_TIDY)
  message(STATUS "use clang-tidy")
  find_program(CMAKE_CXX_CLANG_TIDY
    NAMES clang-tidy clang-tidy-8 clang-tidy-9 clang-tidy-7
    HINTS /usr/local/bin /opt/local/bin
  )
else()
  unset(CMAKE_CXX_CLANG_TIDY CACHE)
endif()

if(NOT CONAN_COMPILER)
  if(NOT ASIO_STANDALONE)
    #####################################################################
    # use boost if found
    option(Boost_USE_MULTITHREADED "Set to OFF to use the non-multithreaded" ON)
    option(Boost_DEBUG "Set to ON to enable debug output from FindBoost." OFF)
    option(Boost_DETAILED_FAILURE_MSG "Set to ON to get detailed information" ON)
    # Set Boost_NO_BOOST_CMAKE to ON to disable the search for boost-cmake.
    set(Boost_NO_BOOST_CMAKE ON)
    set(BOOST_ROOT "/opt/local" CACHE PATH "path to the boost lib")
    set(Boost_USE_STATIC_LIBS OFF)
    if(WIN32)
      set(Boost_USE_STATIC_RUNTIME OFF)
    endif()
    find_package(Boost 1.67
        COMPONENTS atomic chrono coroutine context thread system
    )
    #####################################################################

    if(Boost_FOUND)
      include_directories(${Boost_INCLUDE_DIRS})
      link_directories(${Boost_LIBRARY_DIRS})
      list(APPEND CPPdefinitions BOOST_ALL_NO_LIB=1
        BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING
      )
      list(APPEND EXTRA_LIBS ${Boost_LIBRARIES})
    endif()
    #####################################################################
  endif()


  #######################################################################
  # use openssl if found
  set(OPENSSL_ROOT_DIR "/usr" CACHE PATH "path to the openssl lib")
  find_package(OpenSSL HINTS ${OPENSSL_ROOT_DIR} /opt/local)
  #######################################################################

  if(OpenSSL_FOUND)
    list(APPEND EXTRA_LIBS ${openSSL_LIBRARIES})
    message(STATUS "HAVE_OPENSSL")
    list(APPEND CPPdefinitions HAVE_OPENSSL)
    include_directories(BEFORE ${openSSL_INCLUDE_DIR})
  # else()
  #   find_library(OpenSSL_LIBRARY ssl HINTS ${OpenSSL_ROOT_DIR}/lib)
  #   find_library(CRYPTO_LIBRARY crypto HINTS ${OpenSSL_ROOT_DIR}/lib)
  #   if(OpenSSL_LIBRARY AND CRYPTO_LIBRARY)
  #     set(OpenSSL_FOUND ON)
  #     message(STATUS "USE CRYPTO_LIBRARY")
  #     list(APPEND CPPdefinitions HAVE_OPENSSL)
  #     list(APPEND EXTRA_LIBS ${OpenSSL_LIBRARY} ${CRYPTO_LIBRARY})
  #     include_directories(BEFORE ${OpenSSL_ROOT_DIR}/include)
  #   endif()
  endif()
  #######################################################################
else()
  message(STATUS "ASIO USE CONAN_TARGETS")
  if(CONAN_LIBS_OPENSSL)
    list(APPEND CPPdefinitions HAVE_OPENSSL)
    message(STATUS "have CONAN_LIBS_OPENSSL")
  endif()
endif()


if(UNIX)
  find_library(PTHREAD_LIBRARY pthread REQUIRED)
  message(STATUS "USE ${PTHREAD_LIBRARY}")
  list(APPEND EXTRA_LIBS ${PTHREAD_LIBRARY})
endif()

message(STATUS "CPPdefinitions:${CPPdefinitions}")


# Installation configuration
include(GNUInstallDirs)
set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/asio)

# Interface library
add_subdirectory(include)


if($ENV{CONAN_RUN_TESTS})
  set(ASIO_BUILD_TESTS ON CACHE BOOL "" FORCE)
  message(STATUS "ASIO CONAN_RUN_TESTS")
endif()


if(ASIO_BUILD_EXAMPLES OR ASIO_BUILD_TESTS)
  # enable ctest
  enable_testing()

  # Sources: examples, tests
  add_subdirectory(src)
endif()


# Installation
configure_file(asio-config-version.cmake.in asio-config-version.cmake @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/asio-config-version.cmake
  DESTINATION ${ConfigPackageLocation})
install(EXPORT asio-targets
  DESTINATION ${ConfigPackageLocation}
  NAMESPACE asio::)
install(FILES asio-config.cmake DESTINATION ${ConfigPackageLocation})

