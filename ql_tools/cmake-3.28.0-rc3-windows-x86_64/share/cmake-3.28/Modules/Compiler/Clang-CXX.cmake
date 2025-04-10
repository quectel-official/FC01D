include(Compiler/Clang)
__compiler_clang(CXX)
__compiler_clang_cxx_standards(CXX)

if("x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xGNU")
  if((NOT DEFINED CMAKE_DEPENDS_USE_COMPILER OR CMAKE_DEPENDS_USE_COMPILER)
      AND CMAKE_GENERATOR MATCHES "Makefiles|WMake"
      AND CMAKE_DEPFILE_FLAGS_CXX)
    # dependencies are computed by the compiler itself
    set(CMAKE_CXX_DEPFILE_FORMAT gcc)
    set(CMAKE_CXX_DEPENDS_USE_COMPILER TRUE)
  endif()

  set(CMAKE_CXX_COMPILE_OPTIONS_EXPLICIT_LANGUAGE -x c++)
  set(CMAKE_CXX_COMPILE_OPTIONS_VISIBILITY_INLINES_HIDDEN "-fvisibility-inlines-hidden")
endif()

cmake_policy(GET CMP0025 appleClangPolicy)
if(APPLE AND NOT appleClangPolicy STREQUAL NEW)
  return()
endif()

if("x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC")
  set(CMAKE_CXX_CLANG_TIDY_DRIVER_MODE "cl")
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE_DRIVER_MODE "cl")
  if((NOT DEFINED CMAKE_DEPENDS_USE_COMPILER OR CMAKE_DEPENDS_USE_COMPILER)
      AND CMAKE_GENERATOR MATCHES "Makefiles"
      AND CMAKE_DEPFILE_FLAGS_CXX)
    set(CMAKE_CXX_DEPENDS_USE_COMPILER TRUE)
  endif()
endif()

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)
  if("x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xGNU")
    string(CONCAT CMAKE_CXX_SCANDEP_SOURCE
      "\"${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}\""
      " -format=p1689"
      " --"
      " <CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS>"
      " -x c++ <SOURCE> -c -o <OBJECT>"
      " -MT <DYNDEP_FILE>"
      " -MD -MF <DEP_FILE>"
      " > <DYNDEP_FILE>")
    set(CMAKE_CXX_MODULE_MAP_FORMAT "clang")
    set(CMAKE_CXX_MODULE_MAP_FLAG "@<MODULE_MAP_FILE>")
    set(CMAKE_CXX_MODULE_BMI_ONLY_FLAG "--precompile")
  endif()
endif()
