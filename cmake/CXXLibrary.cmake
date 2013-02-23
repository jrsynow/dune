#############################################################################
# Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática  #
# Departamento de Engenharia Electrotécnica e de Computadores               #
# Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                           #
#############################################################################
# Author: Ricardo Martins                                                   #
#############################################################################

macro(dune_probe_cxx_lib)
  message(STATUS "")
  message(STATUS "******************************************")
  message(STATUS "***        Probing C/C++ Library       ***")
  message(STATUS "******************************************")

  # LLVM libc++
  if(NOT DUNE_CLIB_NAME)
    check_cxx_source_compiles("#include <cstddef>\n int main(void) { return _LIBCPP_VERSION; }" LLVM_LIBC)
    if(LLVM_LIBC)
      set(DUNE_CLIB_NAME "libc++")
      set(DUNE_CLIB_CANONICAL "libcpp")
      set(DUNE_CLIB_LIBCPP 1)
    endif(LLVM_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # uClibc
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__UCLIBC__ stdio.h UC_LIBC)
    if(UC_LIBC)
      set(DUNE_CLIB_NAME "uClibc")
      set(DUNE_CLIB_CANONICAL "uclibc")
      set(DUNE_CLIB_UC 1)
    endif(UC_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # GNU C library
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__GLIBC__ stdio.h GNU_LIBC)
    if(GNU_LIBC)
      set(DUNE_CLIB_NAME "GNU")
      set(DUNE_CLIB_CANONICAL "glibc")
      set(DUNE_CLIB_GNU 1)
    endif(GNU_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # Newlib
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(_NEWLIB_VERSION stdio.h NEW_LIBC)
    if(NEW_LIBC)
      set(DUNE_CLIB_NAME "Newlib")
      set(DUNE_CLIB_CANONICAL "newlib")
      set(DUNE_CLIB_NEWLIB 1)
    endif(NEW_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # eCos
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__ECOS__ stdio.h ECOS_LIBC)
    if(ECOS_LIBC)
      set(DUNE_CLIB_NAME "eCos")
      set(DUNE_CLIB_CANONICAL "ecos")
      set(DUNE_CLIB_ECOS 1)
    endif(ECOS_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # MinGW C library
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__MINGW32_VERSION stdio.h MINGW_LIBC)
    if(MINGW_LIBC)
      set(DUNE_CLIB_NAME "MinGW")
      set(DUNE_CLIB_CANONICAL "mingw")
      set(DUNE_CLIB_MINGW 1)
    endif(MINGW_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # Microsoft C library
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(_MSC_VER stdio.h MICROSOFT_LIBC)
    if(MICROSOFT_LIBC)
      set(DUNE_CLIB_NAME "Microsoft")
      set(DUNE_CLIB_CANONICAL "microsoft")
      set(DUNE_CLIB_MICROSOFT 1)
    endif(MICROSOFT_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # MacOS X / Darwin library
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__APPLE__ stdio.h APPLE_LIBC)
    if(APPLE_LIBC)
      set(DUNE_CLIB_NAME "Apple")
      set(DUNE_CLIB_CANONICAL "apple")
      set(DUNE_CLIB_APPLE 1)
    endif(APPLE_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # SUN Library
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__sun stdio.h SUN_LIBC)
    if(SUN_LIBC)
      set(DUNE_CLIB_NAME "SUN")
      set(DUNE_CLIB_CANONICAL "sun")
      set(DUNE_CLIB_SUN 1)
    endif(SUN_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # Dinkum Library
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(_HAS_DINKUM_CLIB stdio.h DINKUM_LIBC)
    if(DINKUM_LIBC)
      set(DUNE_CLIB_NAME "Dinkum")
      set(DUNE_CLIB_CANONICAL "dinkum")
      set(DUNE_CLIB_DINKUM 1)
    endif(DINKUM_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # BSD Library
  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__DragonFly__ stdio.h DRAGONFLY_LIBC)
    if(DRAGONFLY_LIBC)
      set(DUNE_CLIB_NAME "BSD")
      set(DUNE_CLIB_CANONICAL "bsd")
      set(DUNE_CLIB_BSD 1)
    endif(DRAGONFLY_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__FreeBSD__ stdio.h FREEBSD_LIBC)
    if(FREEBSD_LIBC)
      set(DUNE_CLIB_NAME "BSD")
      set(DUNE_CLIB_CANONICAL "bsd")
      set(DUNE_CLIB_BSD 1)
    endif(FREEBSD_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__OpenBSD__ stdio.h OPENBSD_LIBC)
    if(OPENBSD_LIBC)
      set(DUNE_CLIB_NAME "BSD")
      set(DUNE_CLIB_CANONICAL "bsd")
      set(DUNE_CLIB_BSD 1)
    endif(OPENBSD_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  if(NOT DUNE_CLIB_NAME)
    check_symbol_exists(__NetBSD__ stdio.h NETBSD_LIBC)
    if(NETBSD_LIBC)
      set(DUNE_CLIB_NAME "BSD")
      set(DUNE_CLIB_CANONICAL "bsd")
      set(DUNE_CLIB_BSD 1)
    endif(NETBSD_LIBC)
  endif(NOT DUNE_CLIB_NAME)

  # Unknown C library
  if(NOT DUNE_CLIB_NAME)
    set(DUNE_CLIB_NAME "Unknown")
    set(DUNE_CLIB_CANONICAL "unk")
    set(DUNE_CLIB_UNKNOWN 1)
  endif(NOT DUNE_CLIB_NAME)
endmacro(dune_probe_cxx_lib)