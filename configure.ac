# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# $Id: configure.ac,v 1.46 2010-07-21 22:05:35 twu Exp $

AC_PREREQ([2.71])

## We want to get the version number from file 'VERSION' (rather than
## specifying the version info in 'configure.ac'.  Hence, we need a bit
## of M4 magic.  Note that M4 esyscmd has a trailing newline because the
## shell command output has one, hence the patsubst() trick.
#m4_define([PKG_VERSION],
#          m4_bpatsubst(m4_esyscmd([cat VERSION]),
#                       [\([0-9.]*\)\(\w\|\W\)*],
#                       [\1]))
#m4_define([PKG_VERSION],m4_esyscmd([cat VERSION]))
m4_define([PKG_VERSION],esyscmd([cat VERSION]))

AC_INIT([gmap],[PKG_VERSION],[Thomas Wu <twu@gene.com>])
AC_MSG_CHECKING(package version)
AC_MSG_RESULT(PKG_VERSION)


### Read defaults

# Handle CONFIG_SITE better than Autoconf does
for ac_site_file in $CONFIG_SITE; do
  if { (eval echo $ac_site_file | grep '^\./' >/dev/null 2>&1)
       ac_status=$?
       (exit $ac_status); }; then
    # ac_site_file starts with ./
    if test -r "$ac_site_file"; then
      ac_site_file="$ac_site_file"
      echo "really loading site script $ac_site_file: file was found"
      . "$ac_site_file"
      ax_user_site_file_loaded=yes
    else
      AC_MSG_ERROR([cannot find $ac_site_file])
    fi
  else
    # ac_site_file does not start with ./
    if test -r "./$ac_site_file"; then
      echo "really loading site script ./$ac_site_file: file was found"
      . "./$ac_site_file"
      ax_user_site_file_loaded=yes
    elif test -r "$ac_site_file"; then
      echo "really loading site script $ac_site_file: file was found"
      . "$ac_site_file" 2>/dev/null
      ax_user_site_file_loaded=yes
    else
      AC_MSG_ERROR([cannot find $ac_site_file])
    fi
  fi
done

if test x"$ax_user_site_file_loaded" != x"yes"; then
   if test -r "./config.site"; then
     echo "loading default site script ./config.site"
     . "./config.site"
   fi
fi


# Set default CFLAGS if not already set by user
AC_MSG_CHECKING(CFLAGS)
AC_ARG_VAR([CFLAGS], [Compiler flags (default: -O3 -fomit-frame-pointer)])
if test x"$CFLAGS" = x; then
        AC_MSG_RESULT(not set by user so using default -O3 -fomit-frame-pointer)
        ACX_EXPAND(CFLAGS,'-O3 -fomit-frame-pointer')
else
	AC_MSG_RESULT($CFLAGS)
fi


AC_CONFIG_SRCDIR([src/gmap.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_AUX_DIR([config])
# For autotest: AC_CONFIG_TESTDIR([tests],[tests:src:util])

AC_CONFIG_MACRO_DIR([config])

AC_SUBST([LIBGMAP_SO_VERSION], [1:0:0])
#AC_SUBST([LIBGMAP_API_VERSION], [1.0])

AC_CANONICAL_TARGET

AC_MSG_CHECKING(for non-Intel CPU)
case $host_cpu in
     powerpc64le)
     AC_MSG_RESULT(yes, power8)
     AC_DEFINE(AX_HOST_POWER8,1,[Define to 1 if host is Power8])
     ;;

     x86_64) 
     AC_MSG_RESULT(no)
     ;;

     *)
     AC_MSG_RESULT(no)
     ;;
esac


AC_SYS_LARGEFILE
AC_ARG_PROGRAM

#AM_INIT_AUTOMAKE([no-dependencies])
#AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE([disable])


AM_CONDITIONAL(FULLDIST,test "x$enable_fulldist" = xyes)
AC_ARG_ENABLE([fulldist],
	       AS_HELP_STRING([--enable-fulldist],[For use by program maintainer]),
	      [enable_fulldist="yes"],
              [enable_fulldist="no"])

AM_CONDITIONAL(MAINTAINER,test "x$enable_maintainer" = xyes)
AC_ARG_ENABLE([maintainer],
	       AS_HELP_STRING([--enable-maintainer],[For use by program maintainer]),
	      [enable_maintainer="yes"],
              [enable_maintainer="no"])

#AM_CONDITIONAL(MAKE_LIB,test "x$enable_library" = xyes)
#AC_ARG_ENABLE([lib],
#	       AS_HELP_STRING([--enable-lib],[#                              Build and install a GMAP library (Warning: will put headers into include subdirectory) (default=disable-lib)]),
#	      [enable_library="yes"],
#              [enable_library="no"])

# For Perl scripts, because otherwise configure inserts 
# @bindir@ => "${exec_prefix}/bin";
AC_MSG_CHECKING(bindir)
ACX_EXPAND(BINDIR,$bindir)
AC_SUBST(BINDIR)
AC_MSG_RESULT($BINDIR)



# Checks for programs.
#AC_PATH_PROG([PERL],[perl])
ACX_PATH_PERL

AC_PROG_CC
AM_PROG_CC_C_O
# AC_PROG_LIBTOOL  -- No longer working
AX_COMPILER_VENDOR   # Returns gnu, intel, ...
# AM_CONDITIONAL(INTEL_COMPILER,test "x$ax_cv_c_compiler_vendor" = xintel)


# Checks for libraries.
AC_CHECK_LIB([m], rint)

#AC_CHECK_LIB(popt, poptGetContext, [answer="yes"], [answer="no"])
#if test x"$answer" == xyes; then
#	AC_DEFINE([HAVE_LIBPOPT],[1],[Define to 1 if you have the 'popt' library (-lpopt)])
#	POPT_LIBS="-lpopt"
#else
#	POPT_LIBS=""
#fi
#AC_SUBST(POPT_LIBS)


# In call to ACX_PTHREAD, don't provide ACTION-IF-FOUND; otherwise,
# HAVE_PTHREAD won't be defined
AC_MSG_CHECKING(for pthreads feature)
AC_ARG_ENABLE([pthreads],
              AS_HELP_STRING([--enable-pthreads],[Enable pthreads (default=yes)]),
              [answer="$enableval"],
              [answer=""])
case x"$answer" in
	xyes)
        AC_MSG_RESULT(enabled)
	ACX_PTHREAD([],[pthread_warning=yes])
	;;
	
	xno)
	AC_MSG_RESULT(disabled)
	;;

	x)
	AC_MSG_RESULT([not specified so enabled by default])
	ACX_PTHREAD([],[pthread_warning=yes])
	;;
esac	


# Checks for header files.
AC_HEADER_DIRENT
AC_CHECK_HEADERS([fcntl.h limits.h stddef.h stdlib.h string.h strings.h unistd.h sys/types.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
if test "$ac_cv_c_line" != no; then
  AC_DEFINE(HAVE_INLINE,1,[Define to 1 if compiler supports extern inline])
  AC_SUBST(HAVE_INLINE)
fi

AC_C_BIGENDIAN
AC_C_CONST
AC_C_VOLATILE
AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_CHECK_TYPES([caddr_t])
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(unsigned long long)
AC_CHECK_SIZEOF(off_t)


# Checks for library functions.
#AC_FUNC_MALLOC -- Causes use of rpl_malloc
#AC_FUNC_REALLOC -- Causes use of rpl_realloc
#AC_FUNC_CLOSEDIR_VOID -- We are checking return value

AC_FUNC_FSEEKO

#AC_FUNC_MMAP        # Checks only private fixed mapping of already-mapped memory


AC_MSG_CHECKING(whether alloca is enabled)
AC_ARG_ENABLE([alloca],
	      AS_HELP_STRING([--enable-alloca],[Enable alloca (default=no) for stack-based memory allocation.  Warning: Can cause GMAP to crash on long sequences.]),
              [answer="$enableval"],
              [answer=""])
case x"$answer" in
     xyes)
     AC_MSG_RESULT(enabled)
     AC_FUNC_ALLOCA
     ;;

     xno)
     AC_MSG_RESULT(disabled)
     ;;

     x)
     AC_MSG_RESULT([not specified so disabled by default])
     ;;
esac	


AC_MSG_CHECKING(whether mmap is enabled)
AC_ARG_ENABLE([mmap],
	      AS_HELP_STRING([--enable-mmap],[Enable mmap (default=yes).  Recommend that you do not disable this unless absolutely necessary.]),
              [answer="$enableval"],
              [answer=""])
case x"$answer" in
     xyes)
     AC_MSG_RESULT(enabled)
     ACX_MMAP_FIXED   # Checks fixed mapping
     ACX_MMAP_VARIABLE   # Checks variable mapping
     ;;

     xno)
     AC_MSG_RESULT(disabled)
     ;;

     x)
     AC_MSG_RESULT([not specified so enabled by default])
     ACX_MMAP_FIXED   # Checks fixed mapping
     ACX_MMAP_VARIABLE   # Checks variable mapping
     ;;
esac	


if test x"$acx_mmap_fixed_ok" = xyes; then
  AC_DEFINE(HAVE_MMAP,1,[Define to 1 if you have a working 'mmap' system call.])
else
  if test x"$acx_mmap_variable_ok" = xyes; then
    AC_DEFINE(HAVE_MMAP,1,[Define to 1 if you have a working 'mmap' system call.])
  fi
fi

ACX_MMAP_FLAGS
ACX_MADVISE_FLAGS
ACX_SHM_FLAGS

AC_CHECK_FUNCS([ceil floor index log madvise memcpy memmove memset munmap pow rint stat64 strtoul sysconf sysctl sigaction \
                shmget shmctl shmat shmdt semget semctl semop])

ACX_STRUCT_STAT64
ACX_PAGESIZE

ACX_FUNC_FOPEN_BINARY
ACX_FUNC_FOPEN_TEXT


#AC_MSG_CHECKING(for builtin popcount/clz/ctz features)
#AC_ARG_ENABLE([builtin-popcount],
#              AS_HELP_STRING([--enable-builtin-popcount],[#                             Enable -mpopcnt if it __builtin_popcount compiles and runs (default=yes).  May want to disable if compiling on one machine and running on another.]),
#              [answer="$enableval"],
#              [answer=""])
#case x"$answer" in
#	xyes)
#        AC_MSG_RESULT(enabled)
#	ACX_BUILTIN_POPCOUNT
#	;;
#	
#	xno)
#	AC_MSG_RESULT(disabled)
#	;;
#
#	x)
#	AC_MSG_RESULT([not specified so enabled by default])
#	ACX_BUILTIN_POPCOUNT
#	;;
#esac	
#if test x"$acx_mpopcnt_ok" = x"yes"; then
#  POPCNT_CFLAGS="$POPCNT_CFLAGS -mpopcnt"
#fi
#AC_SUBST(POPCNT_CFLAGS)


ACX_BUILTIN_POPCOUNT
ACX_ASM_BSR


ACX_SSE2_SLLI_CONST_IMM8
ACX_STTNI
ACX_SIMD_INTRINSICS

AC_MSG_CHECKING(for whether simd is enabled)
AC_ARG_ENABLE([simd],
	      AS_HELP_STRING([--enable-simd],[Enable simd commands in general if they compile and run (default=yes).]),
              [answer="$enableval"],
              [answer=""])
case x"$answer" in
     xyes)
     AC_MSG_RESULT(enabled)
     ax_cv_want_simd=yes
     ;;

     xno)
     AC_MSG_RESULT(disabled)
     ax_cv_want_simd=no
     ;;

     x)
     AC_MSG_RESULT([not specified so enabled by default])
     ax_cv_want_simd=yes
     ;;
esac	



case $host_cpu in
  arm*|aarch*)
  ;;

  i[[3456]]86*|x86_64*|amd64*)
    AC_DEFINE(USE_INTEL_INTRINSICS,1,[Define to 1 to use Intel SIMD intrinsics rather than SIMDe])
    if test x"$ax_cv_c_compiler_vendor" = xintel; then
      AX_CPUID_INTEL      # Determine CPU characteristics for an Intel compiler
    else
      AX_CPUID_NON_INTEL  # Determine CPU characterics for a non-Intel (e.g., gcc) compiler
    fi
  ;;
esac

AX_EXT   # Sets SIMD_CFLAGS, and HAVE_ARM, HAVE_SSE, HAVE_SSE2,...

if test "x$ax_cv_want_simd" = xno; then
  compile_level=none
elif test "x$ax_make_arm" = xyes; then
  compile_level=arm
elif test "x$ax_make_avx512bw" = xyes; then
  compile_level=avx512bw
elif test "x$ax_make_avx512" = xyes; then
  compile_level=avx512
elif test "x$ax_make_avx2" = xyes; then
  compile_level=avx2
elif test "x$ax_make_sse42" = xyes; then
  compile_level=sse42
elif test "x$ax_make_sse41" = xyes; then
  compile_level=sse41
elif test "x$ax_make_ssse3" = xyes; then
  compile_level=ssse3
elif test "x$ax_make_sse2" = xyes; then
  compile_level=sse2
else
  compile_level=none
fi


# User-selected compile level
AC_ARG_WITH([simd-level],
            AS_HELP_STRING([--with-simd-level=STRING],[User-selected SIMD level (none, arm, sse2, ssse3, sse41/sse4.1, sse42/sse4.2, avx2, avx512, avx512bw)]),
            [answer="$withval"],
            [answer=""])
if test x"$answer" != x; then
	AC_MSG_CHECKING(for user-selected SIMD level)
	compile_level=$answer
	AC_MSG_RESULT($compile_level)
	case $compile_level in
	arm)
	;;
	avx512bw)
	;;
	avx512)
	;;
	avx2)
	;;
	sse42)
	;;
	sse4.2)
	compile_level = sse42;
	;;
	sse41)
	;;
	sse4.1)
	compile_level = sse41;
	;;
	ssse3)
	;;
	sse2)
	;;
	none)
	;;
	*)
	AC_MSG_ERROR([Compiler level $compile_level not recognized.  Allowed values: none, arm, sse2, ssse3, sse4.1 (or sse41), sse4.2 (or sse42), avx2, avx512, avx512bw])
	;;
	esac
fi


AM_CONDITIONAL(MAKE_ARM,[test "$compile_level" = arm])
AM_CONDITIONAL(MAKE_AVX512BW,[test "$compile_level" = avx512bw])
AM_CONDITIONAL(MAKE_AVX512,[test "$compile_level" = avx512])
AM_CONDITIONAL(MAKE_AVX2,[test "$compile_level" = avx2])
AM_CONDITIONAL(MAKE_SSE4_2,[test "$compile_level" = sse42])
AM_CONDITIONAL(MAKE_SSE4_1,[test "$compile_level" = sse41])
AM_CONDITIONAL(MAKE_SSSE3,[test "$compile_level" = ssse3])
AM_CONDITIONAL(MAKE_SSE2,[test "$compile_level" = sse2])



# Genome directory
AC_MSG_CHECKING(gmapdb)
AC_ARG_WITH([gmapdb],
            AS_HELP_STRING([--with-gmapdb=DIR],[Default GMAP database directory]),
            [answer="$withval"],
            [answer=""])
if test x"$answer" = x; then
	ACX_EXPAND(GMAPDB,'${prefix}/share')
else
	GMAPDB=$answer
fi
AC_SUBST(GMAPDB)
AC_MSG_RESULT($GMAPDB)


# MAX_STACK_READLENGTH
# AC_MSG_CHECKING(MAX_STACK_READLENGTH)
# AC_ARG_VAR([MAX_STACK_READLENGTH], [Maximum read length for GSNAP allocating on stack rather than heap (default 300)])
# if test x"$MAX_STACK_READLENGTH" = x; then
#         ACX_EXPAND(MAX_STACK_READLENGTH,'300')
# fi
# AC_MSG_RESULT($MAX_STACK_READLENGTH)


# zlib package

AC_MSG_CHECKING(for zlib support)
AC_ARG_ENABLE([zlib],
              AS_HELP_STRING([--enable-zlib],[Enable zlib support (option needed for uncompressing gzip files) (default=yes)]),
              [answer="$enableval"],
              [answer=""])
case x"$answer" in
   xno)
   AC_MSG_RESULT(disabled)
   ZLIB_LIBS=""
   have_zlib=no
   ;;

   *)
   AC_MSG_RESULT(enabled)
   failed=0;
   passed=0;
   AC_CHECK_HEADER(zlib.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_CHECK_LIB(z,gzopen,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_CHECK_LIB(z,gzeof,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_CHECK_LIB(z,gzgetc,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_CHECK_LIB(z,gzgets,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_CHECK_LIB(z,gzclose,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_MSG_CHECKING(if zlib package is complete)
   if test $failed -gt 0
   then
      AC_MSG_RESULT(no -- some components failed test)
      ZLIB_LIBS=""
      have_zlib=no
   else
      AC_MSG_RESULT(working)
      AC_DEFINE(HAVE_ZLIB,1,[Define to 1 if you have a working zlib library.])
      ZLIB_LIBS="-lz"
      have_zlib=yes

      AC_CHECK_LIB(z,gzbuffer,have_gzbuffer=1,have_gzbuffer=0)
      if test $have_gzbuffer -gt 0
      then
        AC_DEFINE(HAVE_ZLIB_GZBUFFER,1,[Define to 1 if your zlib library has a gzbuffer function.])
      fi
   fi
   ;;

esac
AC_SUBST(ZLIB_LIBS)


# bzip2 package

AC_MSG_CHECKING(for bzlib support)
AC_ARG_ENABLE([bzlib],
              AS_HELP_STRING([--enable-bzlib],[Enable bzlib support (option needed for uncompressing bzip2 files) (default=yes)]),
              [answer="$enableval"],
              [answer=""])
case x"$answer" in
   xno)
   AC_MSG_RESULT(disabled)
   BZLIB_LIBS=""
   have_bzlib=no
   ;;

   *)
   AC_MSG_RESULT(enabled)
   failed=0;
   passed=0;
   AC_CHECK_HEADER(bzlib.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_CHECK_LIB(bz2,BZ2_bzReadOpen,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_CHECK_LIB(bz2,BZ2_bzRead,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_CHECK_LIB(bz2,BZ2_bzReadClose,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
   AC_MSG_CHECKING(if bzlib package is complete)
   if test $failed -gt 0
   then
      AC_MSG_RESULT(no -- some components failed test)
      BZLIB_LIBS=""
      have_bzlib=no
   else
      AC_MSG_RESULT(working)
      AC_DEFINE(HAVE_BZLIB,1,[Define to 1 if you have a working bzlib library.])
      BZLIB_LIBS="-lbz2"
      have_bzlib=yes
   fi
   ;;

esac
AC_SUBST(BZLIB_LIBS)


AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
#AC_CONFIG_FILES([lib/gmap-${LIBGMAP_API_VERSION}.pc:lib/gmap.pc.in])
AC_CONFIG_FILES([util/Makefile])
AC_CONFIG_FILES([util/gmap_process.pl])
AC_CONFIG_FILES([util/gmap_build.pl])
AC_CONFIG_FILES([util/gmap_cat.pl])
AC_CONFIG_FILES([util/md_coords.pl])
AC_CONFIG_FILES([util/fa_coords.pl])
AC_CONFIG_FILES([util/psl_splicesites.pl])
AC_CONFIG_FILES([util/psl_introns.pl])
AC_CONFIG_FILES([util/psl_genes.pl])
AC_CONFIG_FILES([util/ensembl_genes.pl])
AC_CONFIG_FILES([util/gtf_splicesites.pl])
AC_CONFIG_FILES([util/gtf_transcript_splicesites.pl])
AC_CONFIG_FILES([util/gtf_introns.pl])
AC_CONFIG_FILES([util/gtf_genes.pl])
AC_CONFIG_FILES([util/gff3_splicesites.pl])
AC_CONFIG_FILES([util/gff3_introns.pl])
AC_CONFIG_FILES([util/gff3_genes.pl])
AC_CONFIG_FILES([util/dbsnp_iit.pl])
AC_CONFIG_FILES([util/gvf_iit.pl])
AC_CONFIG_FILES([util/vcf_iit.pl])
AC_CONFIG_FILES([tests/Makefile])
AC_CONFIG_FILES([tests/align.test],[chmod +x tests/align.test])
AC_CONFIG_FILES([tests/coords1.test],[chmod +x tests/coords1.test])
AC_CONFIG_FILES([tests/setup1.test],[chmod +x tests/setup1.test])
AC_CONFIG_FILES([tests/iit.test],[chmod +x tests/iit.test])

AC_OUTPUT

if test x"$perl_warning" = xyes; then
  AC_MSG_WARN([

WARNING: You don't seem to have perl installed with working
versions of the IO::File and Getopt::Std modules.  If you know
where such a version of Perl exists, please specify its full
path under PERL in the config.site file and run configure again.
])
fi

if test x"$pthread_warning" = xyes; then
  AC_MSG_WARN([

WARNING: You don't seem to have headers and libraries for pthreads.
GMAP will work without these, but the program can run faster on
multiprocessor machines if POSIX threads are available.  If you wish
to enable pthreads, please specify the appropriate values for CC,
PTHREAD_CFLAGS, and PTHREAD_LIBS in the config.site file and run
configure again.  Otherwise, you may proceed with the rest of the
installation.
])
fi

if test x"$acx_mmap_fixed_ok" != xyes; then
  if test x$"acx_mmap_variable_ok" != xyes; then
    AC_MSG_WARN([

WARNING: You don't seem to have memory mapping.  GMAP will work
without this, but the program can run faster if memory mapping is
available.  If you wish to enable memory mapping, please specify the
appropriate values for CC, PTHREAD_CFLAGS, and PTHREAD_LIBS in the
config.site file and run configure again.  Otherwise, you may proceed
with the rest of the installation.
])
  fi
fi

# if test x"$ax_cv_sse2_slli_const_imm8" = xyes; then
#   AC_MSG_WARN([
# 
# WARNING: Your C compiler does not know how to handle a particular
# form of the SSE2 shift commands, and must be somewhat old (probably
# gcc 4.1 or gcc 4.2).  The configure script has therefore substituted
# alternate C code that your compiler can handle.  However, for 
# optimal performance, you may want to obtain a more recent C compiler
# and build the code with that.
# ])
# fi

if test x"$ax_cv_ext_compiler_problem" = x"yes"; then
  AC_MSG_WARN([

WARNING: Your C compiler appears not to take advantage of all of the
SIMD features available on your computer, and must be somewhat old.
The configure script has therefore substituted alternate C code that
your compiler can handle.  However, for optimal performance, you may
want to obtain a more recent C compiler and build the code with that.
Here are the compiler flags to be used for SIMD:
])
fi

if test x"$ax_cv_ext_linker_problem" = x"yes"; then
  AC_MSG_WARN([

WARNING: Your C linker appears not to work on SIMD header files that
your C compiler accepts, so there appears to be some incompatibility
between your compiler and linker.  The configure script has therefore
substituted alternate C code that your linker can handle.  However,
for optimal performance, you may need to set your LDFLAGS or
LD_LIBRARY_PATH environment variable to the correct directory, or
rebuild your compiler and linker.
])
fi

AC_MSG_CHECKING(standard compiler and flags to be used)
AC_MSG_RESULT($CC $CFLAGS)
AC_MSG_CHECKING(standard linker flags to be used)
AC_MSG_RESULT($LD_FLAGS)

AC_MSG_CHECKING(pthread compiler flags to be used)
AC_MSG_RESULT($PTHREAD_CFLAGS)

AC_MSG_CHECKING(popcnt flags to be used)
AC_MSG_RESULT($POPCNT_CFLAGS)

AC_MSG_CHECKING(compile level)
AC_MSG_RESULT($compile_level)


if test "$compile_level" = arm; then
   AC_MSG_CHECKING(ARM compiler flags to be used)
   AC_MSG_RESULT($SIMD_ARM_CFLAGS)
fi

# Re-using SIMD_AVX512_CFLAGS here
if test "$compile_level" = avx512bw; then
   AC_MSG_CHECKING(AVX512BW compiler flags to be used)
   AC_MSG_RESULT($SIMD_AVX512_CFLAGS)
fi

if test "$compile_level" = avx512; then
   AC_MSG_CHECKING(AVX512 compiler flags to be used)
   AC_MSG_RESULT($SIMD_AVX512_CFLAGS)
fi

if test "$compile_level" = avx2; then
   AC_MSG_CHECKING(AVX2 compiler flags to be used)
   AC_MSG_RESULT($SIMD_AVX2_CFLAGS)
fi

if test "$compile_level" = sse42; then
   AC_MSG_CHECKING(SSE4.2 compiler flags to be used)
   AC_MSG_RESULT($SIMD_SSE4_2_CFLAGS)
fi

if test "$compile_level" = sse41; then
   AC_MSG_CHECKING(SSE4.1 compiler flags to be used)
   AC_MSG_RESULT($SIMD_SSE4_1_CFLAGS)
fi

if test "$compile_level" = ssse3; then
   AC_MSG_CHECKING(SSSE3 compiler flags to be used)
   AC_MSG_RESULT($SIMD_SSSE3_CFLAGS)
fi

if test "$compile_level" = sse2; then
   AC_MSG_CHECKING(SSE2 compiler flags to be used)
   AC_MSG_RESULT($SIMD_SSE2_CFLAGS)
fi


#AC_MSG_CHECKING(SIMD features available on computer)
#AC_MSG_RESULT($ax_cv_cpu_features)
#AC_MSG_CHECKING(SIMD compiler flags to be used)
#AC_MSG_RESULT($SIMD_CFLAGS)

