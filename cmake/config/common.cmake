include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckTypeSize)
include(FindPkgConfig)
include(CTest)
find_package(Threads REQUIRED)

pkg_check_modules(CHECK check)

CHECK_INCLUDE_FILE(sys/mman.h HAVE_SYS_MMAN_H)

set(VMAJ ${PROJECT_VERSION_MAJOR})
set(VMIN ${PROJECT_VERSION_MINOR})
set(VMIC ${PROJECT_VERSION_PATCH})
set(VREV ${PROJECT_VERSION_TWEAK})
set(EFL_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(EFL_VERSION_MINOR ${PROJECT_VERSION_MINOR})

EFL_OPTION(BUILD_PROFILE "How to build EFL" release CHOICE release;dev;debug)

EFL_OPTION(EFL_NOLEGACY_API_SUPPORT "Whenever to disable non-Eo (Legacy) APIs" ON)
EFL_OPTION(EFL_EO_API_SUPPORT "Whenever to enable new Eo APIs" ON)
if(NOT EFL_EO_API_SUPPORT)
  EFL_OPTION_SET_MESSAGE(EFL_EO_API_SUPPORT "Risky!")
endif()

#this is needed to build efl
set(EFL_BETA_API_SUPPORT 1)