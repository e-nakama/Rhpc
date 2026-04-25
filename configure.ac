AC_PREREQ(2.62)
AC_INIT(Rhpc, 0.18, nakama@ki.rim.or.jp)

AC_CONFIG_HEADER(src/common/config.h)

AC_CHECK_HEADERS([sched.h sys/types.h unistd.h dlfcn.h])
AC_CHECK_FUNCS([sched_getscheduler])

AC_MSG_CHECKING(for dladdr)
AC_TRY_COMPILE(
  [
#define _GNU_SOURCE
#include <dlfcn.h>
  ],[
Dl_info* info = 0;
dladdr(0, info);  
  ],[
AC_DEFINE([HAVE_DLADDR],[1],[this system have a dladdr in dlfcn.h])
AC_MSG_RESULT(yes)
  ],[
AC_MSG_RESULT(no)
  ])

R_HOME=`echo ${R_HOME}`

CHKLIBR=`${R_HOME}/bin${R_ARCH}/R CMD config --ldflags 2>/dev/null`
have_libR=no
if test -n "$CHKLIBR"; then
   have_libR=yes
fi
AC_MSG_CHECKING([libR check])
if test "${have_libR}" = no; then
	AC_MSG_RESULT(no)
	AC_MSG_ERROR([libR not found.
!!! Rhpc need R library. 
!!! Please add option --enable-R-shlib or
!!! --enable-R-static-lib rebuild R
])
else
	AC_MSG_RESULT(yes)
fi

CC=`${R_HOME}/bin${R_ARCH}/R CMD config CC 2>/dev/null`
CFLAGS=`${R_HOME}/bin${R_ARCH}/R CMD config CFLAGS 2>/dev/null`
LDFLAGS=`${R_HOME}/bin${R_ARCH}/R CMD config LDFLAGS 2>/dev/null`

AC_ARG_WITH(mpicc,
    AC_HELP_STRING([--with-mpicc=MPI_C_WRAPPER_COMPILER],
                   [MPI C wrapper compiler name or C compiler name.]),
[   if test "${withval}x" != x; then
         MPICC=${withval}
    fi  ]	
)
AC_ARG_WITH(mpi-cflags,
    AC_HELP_STRING([--with-mpi-cflags=ARGS],
                   [MPI CFLAGS.(cf. "-I/usr/include/openmpi")]),
[   if test "${withval}x" != x; then
         MPI_CFLAGS=${withval}
    fi  ]	
)
AC_ARG_WITH(mpi-ldflags,
    AC_HELP_STRING([--with-mpi-ldflags=ARGS],
                   [MPI LDFLAGS.(cf. "-L/usr/lib/openmpi -lmpi")]),
[   if test "${withval}x" != x; then
         MPI_LDFLAGS=${withval}
    fi  ]	
)
AC_ARG_WITH(package-dir,
    AC_HELP_STRING([--with-package-dir=DIR],
                   [Top directory of Rhpc package. hard code does the worker-path with the shell in Rhpc and RhpcSpawn. (maybe need binary package build of distribution)]),
[   if test "${withval}x" != x; then
        MY_PACKAGE_DIR=${withval}
    fi  ]
)

if test -z "${MPICC}" ; then
   AC_CHECK_PROGS(MPICC, mpicc hcc mpxlc_r mpxlc mpcc cmpicc mpifcc)
fi

if test ! -z "${MPICC}" && test -z "${MPI_CFLAGS}" ; then
   MPI_CFLAGS=`${MPICC} -showme:compile 2>/dev/null`
   MPI_LDFLAGS=`${MPICC} -showme:link 2>/dev/null`
fi

if test ! -z "${MPICC}" && test -z "${MPI_CFLAGS}" ; then
   MPI_CFLAGS=`${MPICC} -showme 2>/dev/null`
   MPI_LDFLAGS=`${MPICC} -showme 2>/dev/null`
fi

if test ! -z "${MPICC}" && test -z "${MPI_CFLAGS}" ; then
   MPI_CFLAGS=`${MPICC} -show  2>/dev/null| xargs -n 1|sed -e '1d'|xargs`
   MPI_LDFLAGS=`${MPICC} -show  2>/dev/null| xargs -n 1|sed -e '1d'|xargs`
fi

if test -z "${MPI_CFLAGS}" && test ! -z "${MPI_ROOT}"; then
   MPI_CFLAGS="-I${MPI_ROOT}/include"
   MPI_LDFLAGS="-L${MPI_ROOT}/lib -lmpi"
fi

CFLAGS="$CFLAGS $MPI_CFLAGS"
LDFLAGS="$LDFLAGS $MPI_LDFLAGS"
save_LIBS="$LIBS"    # Ubuntu 14.04 gcc-4.8  fake...
LIBS="$MPI_LDFLAGS"  # Ubuntu 14.04 gcc-4.8  fake...
AC_MSG_CHECKING([Compiling of MPI C programs])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <mpi.h>]],
[[  MPI_Init(0,(void *)0);
    MPI_Finalize();]])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([could not compile MPI program!
!!! This package requires MPI environment.
!!! See config.log for details])
    have_mpi_h=no]
)
					  
AC_MSG_CHECKING([Linking of MPI C programs])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <mpi.h>]],
                 [[ MPI_Init(0, (void *)0);
                    MPI_Finalize();]])],
                 [AC_DEFINE([HAVE_MPI], 1, [MPI support])
                  have_mpi=yes
                  have_mpi_header=yes],
                 [have_mpi=no])
AC_MSG_RESULT($have_mpi)
if test "$have_mpi" = "no" ; then
   AC_MSG_ERROR([can not link MPI library.
!!! This package requires MPI environment.
!!! See config.log for details])
fi

LIBS="$saveLIBS" # Ubuntu 14.04 gcc-4.8  fake...


if test -z "${MY_PACKAGE_DIR}" ; then
	if test "${RPM_BUILD_ROOT}x" != "x"; then
		MY_PACKAGE_DIR=${R_HOME}/library/Rhpc
	else
		#MY_PACKAGE_DIR=${R_PACKAGE_DIR}
		MY_PACKAGE_DIR=`${R_HOME}/bin/Rscript -e 'cat(grep("^(?!.*00LOCK).*$",.libPaths(),value=TRUE,perl=T)[[1]])'`/${R_PACKAGE_NAME}
	fi
fi

R_HOME=${R_HOME}
R_ARCH=${R_ARCH}
RCMDCONFIGLDFLAGS=$(${R_HOME}/bin/R CMD config --ldflags 2>/dev/null)
RCMDCONFIGCPPFLAGS=$(${R_HOME}/bin/R CMD config --cppflags 2>/dev/null)


AC_SUBST(MPICC)
AC_SUBST(MPI_CFLAGS)
AC_SUBST(MPI_LDFLAGS)
AC_SUBST(LIBR)
AC_SUBST(R_HOME)
AC_SUBST(R_ARCH)
AC_SUBST(RCMDCONFIGLDFLAGS)
AC_SUBST(RCMDCONFIGCPPFLAGS)
AC_SUBST(MY_PACKAGE_DIR)
AC_SUBST(R_PACKAGE_DIR)

AC_OUTPUT(src/Makevars src/worker/Makefile src/worker/Rhpc src/worker/RhpcSpawn)

