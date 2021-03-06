# Copyright 2010 B D Dudson, S Farley
#
# Contact Ben Dudson, bd512@york.ac.uk
#
# This file is part of BOUT++.
#
# BOUT++ is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# BOUT++ is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
#
#####################################################################
#
# Process this file with autoconf to produce a configure script.
#
# Changelog:
# 
# 2010-03-09 Ben Dudson <bd512@york.ac.uk>
#    * Changing to always require FFTW (removing NR routines)
# 

AC_PREREQ(2.59)
AC_INIT(BOUT++, 0.8, bd512@york.ac.uk)

AC_ARG_WITH(checks, [  --with-checks=no/1/2/3  Set run-time checking level])
AC_ARG_WITH(signal, [  --with-signal=no        Disable SEGFAULT handling])
AC_ARG_WITH(track,  [  --with-track            Enable variable tracking])
AC_ARG_WITH(pdb,    [  --with-pdb              Enable support for PDB files])
AC_ARG_WITH(netcdf, [  --with-netcdf           Enable support for netCDF files])
AC_ARG_WITH(debug,  [  --with-debug            Enable all debugging flags])
AC_ARG_WITH(ida,    [  --with-ida=/path/to/ida Use SUNDIALS' IDA solver])
AC_ARG_WITH(cvode,  [  --with-cvode            Use SUNDIALS' CVODE solver])
AC_ARG_WITH(fftw,   [  --with-fftw             Set directory of FFTW3 library])
AC_ARG_WITH(lapack, [  --with-lapack           Use the LAPACK library])
AC_ARG_WITH(petsc,  [  --with-petsc            Enable PETSc interface])

EXTRA_INCS="" # Extra includes
EXTRA_LIBS="" # Extra library flags

file_formats=""  # Record which file formats are being supported

AC_ARG_VAR(CFLAGS,[Extra compile flags])

AC_SUBST(EXTRA_INCS)
AC_SUBST(EXTRA_LIBS)

# Adding variables for additional sources
AC_SUBST(PVODE)
AC_SUBST(SOLVER_SOURCE)
AC_SUBST(PRECON_SOURCE)
AC_SUBST(FILEIO_SOURCE)

if test "$with_debug" != ""
then
	echo "Enabling all debug options"
	with_checks="3"
	CFLAGS="$CFLAGS -g"
else
	CFLAGS="$CFLAGS -O"
fi

if test "$with_checks" != "no"
then
	echo "Run-time checking enabled"
	if test "$with_checks" = "1"
	then
		echo " -> Level 1 (Basic checking)"
		CFLAGS="$CFLAGS -DCHECK=1"
	elif test "$with_checks" = "3"
	then
		CFLAGS="$CFLAGS -DCHECK=3"
		echo " -> Level 3 (Full checking + stack tracing)"
	else
		echo " -> Level 2 (Basic checking + stack tracing)"
		CFLAGS="$CFLAGS -DCHECK=2"
	fi
else
	echo "Run-time checking disabled"
fi

if test "$with_signal" != "no"
then
	echo "Segmentation fault handling enabled"
	CFLAGS="$CFLAGS -DSIGHANDLE"
else
	echo "Segmentation fault handling disabled"
fi

if test "$with_track" != ""
then   
       CFLAGS="$CFLAGS -DTRACK"
fi

#####################################################################
# PETSc library

# AC_ARG_VAR(PETSC,[Portable, Extensible Toolkit for Scientific Computation])

if test "$with_petsc" != "" && test "$with_petsc" != "no"
then
### This still needs to check for PETSC_ARCH
	if test "$with_petsc" != "yes"
	then
	PETSC="$with_petsc"
	else
	AC_CHECK_FILE($PETSC_DIR/$PETSC_ARCH/conf, PETSC="include \$(PETSC_DIR)/conf/variables",
	echo "Error - can't find PETSc distribution"
	exit)
	fi
	
cat <<MFILE > petscmake$$
PETSC_ARCH = ${PETSC_ARCH}
PETSC_DIR  = ${PETSC_DIR}
include ${PETSC_DIR}/conf/rules
include ${PETSC_DIR}/conf/variables
echo_cc:
	-@echo \${CC}
echo_cxx:
	-@echo \${CXX}
echo_sundials:
	-@echo \${SUNDIALS_LIB}
MFILE
	HAS_SUNDIALS=`make -f petscmake$$ echo_sundials | grep -c sundials`
	
	if test "$HAS_SUNDIALS" == "0"
	then
		echo "Error - PETSc must have SUNDIALS support"
		rm petscmake$$
		exit
	fi
	
	CC="`make -f petscmake$$ echo_cc`"
	CXX="`make -f petscmake$$ echo_cxx`"
	MPICC=$CC
	MPICXX=$CXX
	
	rm petscmake$$
	
	CFLAGS="$CFLAGS -DPETSC"

else
   PETSC=
fi
AC_SUBST(PETSC, $PETSC)

#############################################################
# Solver choice: SUNDIALS' IDA, SUNDIALS' CVODE, PVODE
#############################################################

if ( ( test "$with_ida" != "" ) && ( test "$with_ida" != "no" ) )
then
	if test "$with_ida" = "yes"
	then
		# No path specified
		echo "SUNDIALS IDA solver enabled"
	else
		# Specified with path
		echo "SUNDIALS IDA solver enabled, path $with_ida"
		EXTRA_INCS="$EXTRA_INCS -I$with_ida/include"
		EXTRA_LIBS="$EXTRA_LIBS -L$with_ida/lib"
	fi
	# Compile in the IDA solver
	SOLVER_SOURCE="$SOLVER_SOURCE ida_solver.cpp"
	EXTRA_LIBS="$EXTRA_LIBS -lsundials_ida -lsundials_nvecparallel"
	
	CFLAGS="$CFLAGS -DIDA" # Used in solver.h. BIT OF A HACK!
else
	if ( ( test "$with_cvode" != "" ) && ( test "$with_cvode" != "no" ) )
	then
		if test "$with_cvode" = "yes"
		then
			# No path specified
			echo "Using SUNDIALS' CVODE solver"
		else
			# Specified with path
			echo "Using SUNDIALS' CVODE solver, path $with_cvode"
			EXTRA_INCS="$EXTRA_INCS -I$with_cvode/include"
			EXTRA_LIBS="$EXTRA_LIBS -L$with_cvode/lib"
		fi
		# Compile in the CVODE solver
		SOLVER_SOURCE="$SOLVER_SOURCE sundials_solver.cpp"
		EXTRA_LIBS="$EXTRA_LIBS -lsundials_cvode -lsundials_nvecparallel"
		
		CFLAGS="$CFLAGS -DCVODE" # Used in solver.h. BIT OF A HACK!
	else
		if test "$PETSC" != ""
		then
			echo "Using PETSc interface"
			SOLVER_SOURCE="$SOLVER_SOURCE petsc_solver.cpp"
			PRECON_SOURCE="$PRECON_SOURCE jstruc.cpp"
			EXTRA_INCS="$EXTRA_INCS \$(PETSC_INCLUDE)"
			EXTRA_LIBS="$EXTRA_LIBS \$(PETSC_LIB)"
		else
			echo "Using PVODE solver"
			# Using the old version of CVODE supplied with BOUT++
			SOLVER_SOURCE="$SOLVER_SOURCE cvode_solver.cpp"
			# Todo: For now, use this PVODE variable until ./configure
			# compiles the library
			PVODE="\$(BOUT_TOP)/PVODE"
			EXTRA_INCS="$EXTRA_INCS -I\$(PVODE)/include -I\$(PVODE)/precon"
			EXTRA_LIBS="$EXTRA_LIBS -L\$(PVODE)/lib -lpvode -lpvpre"
		fi
	fi
fi

#############################################################
# FFT routines
#############################################################

echo "Searching for FFTW library"

if test "$with_fftw" = ""
then
	# No path specified
	FFTPATH=""
	echo " -> Path not given. Searching for library..."
else
	FFTPATH=$with_fftw
	AC_CHECK_FILES( $FFTPATH/include/fftw3.h $FFTPATH/lib/libfftw3.a ,, [
	echo " -> Given path not correct. Finding..."
	FFTPATH=""] )
fi

# Try some known paths
if test "$FFTPATH" = ""
then
	AC_CHECK_FILE(/usr/include/fftw3.h, FFTPATH=/usr,
	AC_CHECK_FILE(/usr/local/include/fftw3.h, FFTPATH=/usr/local,
	AC_CHECK_FILE(/opt/local/include/fftw3.h, FFTPATH=/opt/local,
	AC_CHECK_FILE($HOME/local/include/fftw3.h, FFTPATH=$HOME/local,
	))))
	
	if test "$FFTPATH" != ""
	then
		# Check for the rest of the files
	
		# NOTE: AC_CHECK_FILES works here, but AC_CHECK_FILE doesn't (?!)
		AC_CHECK_FILES( $FFTPATH/lib/libfftw3.a , , FFTPATH="" )
	fi
fi

if test "$FFTPATH" = ""
then
	echo "ERROR: FFTW not found. Required by BOUT++"
	exit
else
	echo " -> path $FFTPATH"
	EXTRA_INCS="$EXTRA_INCS -I$FFTPATH/include"
	EXTRA_LIBS="$EXTRA_LIBS -L$FFTPATH/lib -lfftw3"
fi
echo ""

#############################################################
# LAPACK routines (Used for tri- and band-diagonal solvers)
#############################################################

if ( ( test "$with_lapack" != "" ) && ( test "$with_lapack" != "no" ) )
then
	echo "Using LAPACK"
	EXTRA_LIBS="$EXTRA_LIBS -llapack -lblas"
	CFLAGS="$CFLAGS -DLAPACK"	
fi

#############################################################
# netCDF support
#############################################################

if test "$with_netcdf" != "no"
then
	# Try to find a valid NetCDF installation
	
	echo "Searching for NetCDF library"

	if test "$with_netcdf" = "yes"
	then
		# No path specified. Try using nc-config
		if type nc-config > /dev/null 2>&1; then
			NCPATH=`nc-config --prefix`
		else
			NCPATH=""
		fi
	else
		NCPATH="$with_netcdf"
	fi
	
	# Test if C++ support is included
	if type nc-config > /dev/null 2>&1; then
		if test `nc-config --has-c++` = "yes"
		then
			echo " -> C++ support included"
		else
			echo " -> NO C++ SUPPORT"
			with_netcdf = "no"
		fi
	else
		if type $NCPATH/bin/nc-config > /dev/null 2>&1; then
			if test `$NCPATH/bin/nc-config --has-c++` = "yes"
			then
				echo " -> C++ support included"
			else
				echo " -> NO C++ SUPPORT"
				with_netcdf = "no"
			fi
		else
			echo " -> nc-config not found. Can't check C++ support"
		fi
	fi
fi

if test "$with_netcdf" != "no"
then
	if type nc-config > /dev/null 2>&1
	then
		NCP=`which nc-config`
		echo " -> Using $NCP for configuration settings"
		# Can use nc-config to get configuration options
	 	# Set a compile-time flag
		CFLAGS="$CFLAGS -DNCDF"
	
		# Add extra sources
		FILEIO_SOURCE="$FILEIO_SOURCE nc_format.cpp"
			      
		NCINC=`nc-config --cflags`
		EXTRA_INCS="$EXTRA_INCS $NCINC"
		NCLIB=`nc-config --libs | sed s/-lnetcdf/-lnetcdf_c++\ -lnetcdf/` # Note netcdf_c++ needed
		EXTRA_LIBS="$EXTRA_LIBS $NCLIB" 

		file_formats="$file_formats netCDF"

		echo " -> NetCDF support enabled"
		echo ""
	else
		# No nc-config. Check the NetCDF path manually

		if test "$NCPATH" != ""
		then
			# Path set, so check if libraries present
			AC_CHECK_FILES( $NCPATH/include/netcdfcpp.h $NCPATH/lib/libnetcdf_c++.a $NCPATH/lib/libnetcdf.a ,, [
			echo " -> Given path not correct. Finding..."
			NCPATH=""] )
			else
			echo " -> Path not given. Searching for library..."
		fi
	
		# Try some known paths
		if test "$NCPATH" = ""
		then
			AC_CHECK_FILE(/usr/include/netcdfcpp.h, NCPATH=/usr/,
			AC_CHECK_FILE(/usr/local/include/netcdfcpp.h, NCPATH=/usr/local/,
			AC_CHECK_FILE($HOME/local/include/netcdfcpp.h, NCPATH=$HOME/local,
			)))
		
			if test "$NCPATH" != ""
			then
				# Check for the rest of the files
				AC_CHECK_FILES( $NCPATH/lib/libnetcdf_c++.a $NCPATH/lib/libnetcdf.a, , NCPATH="")
			fi
		fi

		# Try to find using locate
		if test	"$NCPATH" = ""
		then
			NCFILE=`locate netcdfcpp.h | head -n 1`
			if test "$NCFILE" != ""
			then
				NCFILE=`dirname $NCFILE`
				NCFILE=$NCFILE/..
				AC_CHECK_FILES( $NCFILE/include/netcdfcpp.h $NCFILE/lib/libnetcdf_c++.a $NCFILE/lib/libnetcdf.a , NCPATH=$NCFILE, with_netcdf="no")
			else
				echo "checking locate... no"
				with_netcdf="no"
			fi
		fi

		# Finally, if found, add to BOUT++

		if test "$with_netcdf" != "no"
		then
			# Set a compile-time flag
			CFLAGS="$CFLAGS -DNCDF"
	
			# Add extra sources
			FILEIO_SOURCE="$FILEIO_SOURCE nc_format.cpp"
			      
			if test "$NCPATH" != ""; then
				echo " -> path $NCPATH"
				EXTRA_INCS="$EXTRA_INCS -I$NCPATH/include"
				EXTRA_LIBS="$EXTRA_LIBS -L$NCPATH/lib"
			fi
	
			EXTRA_LIBS="$EXTRA_LIBS -lnetcdf_c++ -lnetcdf"

			file_formats="$file_formats netCDF"

			echo " -> NetCDF support enabled"
			echo ""
		fi
	fi
fi

if test "$with_netcdf" = "no"
then
	echo "NetCDF support disabled"
fi

#####################################################################
# PACT library
#####################################################################

AC_ARG_VAR(PACT,[Path to the Portable Application Toolkit library])

if test "$with_pdb" = "no"
then
	echo "PDB support disabled"
else
	echo "Searching for PDB library"
	
	if test "$PACT" != ""
	then
		# Path set, check it's correct
		AC_CHECK_FILES( $PACT/include/pdb.h $PACT/lib/libpdb.a $PACT/lib/libpml.a $PACT/lib/libscore.a ,, [
		echo "PACT library location not correct. Finding..."
		PACT=""] )
	else
		echo "PACT variable not set. Searching for library..." 
	fi

	if ( test "$with_pdb" != "" ) && ( test "$with_pdb" != "yes" ) && ( test "$with_pdb" != "no" )
	then
		# could be a path
		PACT=$with_pdb
	
		AC_CHECK_FILES( $PACT/include/pdb.h $PACT/lib/libpdb.a $PACT/lib/libpml.a $PACT/lib/libscore.a ,, [
		echo "with-pdb library location not correct. Finding..."
		PACT=""] )
	fi

	## Try some known paths
	if test "$PACT" = ""
	then
		AC_CHECK_FILE(/usr/include/pdb.h, PACT=/usr/,
		AC_CHECK_FILE(/usr/local/include/pdb.h, PACT=/usr/local/,
		AC_CHECK_FILE($HOME/local/include/pdb.h, PACT=$HOME/local/,
		AC_CHECK_FILE($HOME/local/include/pdb.h, PACT=$HOME/local/,
		))))

		if test "$PACT" != ""
		then
			# Check the rest of the PACT libraries exist
			AC_CHECK_FILES( $PACT/lib/libpdb.a $PACT/lib/libpml.a $PACT/lib/libscore.a, , PACT="")
		fi
	fi

	## Try to find using locate
	if test "$PACT" = ""
	then
		PACTFILE=`locate pdb.h | head -n 1`
		if test "$PACTFILE" != ""
		then
			PACTFILE=`dirname $PACTFILE`
			PACTFILE=$PACTFILE/..
			AC_CHECK_FILES( $PACTFILE/include/pdb.h $PACTFILE/lib/libpdb.a $PACTFILE/lib/libpml.a $PACTFILE/lib/libscore.a,
			PACT=$PACTFILE, PACT="")
		else
			echo "checking locate... no"
		fi
	fi

	# Test if PACT is set
	if test "$PACT" != ""
	then
		with_pdb="yes"
	
		# Set include and library paths
		EXTRA_INCS="$EXTRA_INCS -I$PACT/include"
		EXTRA_LIBS="$EXTRA_LIBS -L$PACT/lib"
	fi

	## Add PDB support if available
	if test "$with_pdb" = "yes"
	then

		FILEIO_SOURCE="$FILEIO_SOURCE pdb_format.cpp"
		EXTRA_LIBS="$EXTRA_LIBS -lpdb -lpml -lscore \$(shell $PACT/bin/pact -link)"

		# Set a compile-time flag. PDBF because PDB already used in score.h
		CFLAGS="$CFLAGS -DPDBF"

		file_formats="$file_formats PDB"
	else
		echo "PDB support not available"
	fi
fi



#####################################################################
# Check file formats
#####################################################################

if test "$file_formats" = ""
then
	echo "ERROR: At least one file format must be supported"
	exit
else
	echo "Supported file formats:$file_formats"
fi

#####################################################################
# Checks for programs
#####################################################################

#AC_PROG_CXX
#AC_PROG_CC
#AC_PROG_CPP
CC="gcc"
CXX="g++"
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

########################################################
# MPI

AC_DEFUN([ACX_MPI], [
AC_PREREQ(2.50) dnl for AC_LANG_CASE

AC_LANG_CASE([C], [
	AC_REQUIRE([AC_PROG_CC])
	AC_ARG_VAR(MPICC,[MPI C compiler command])
	AC_CHECK_PROGS(MPICC, mpicc hcc mpxlc_r mpxlc mpcc cmpicc cc, $CC)
	acx_mpi_save_CC="$CC"
	CC="$MPICC"
	AC_SUBST(MPICC)
],
[C++], [
	AC_REQUIRE([AC_PROG_CXX])
	AC_ARG_VAR(MPICXX,[MPI C++ compiler command])
	AC_CHECK_PROGS(MPICXX, mpicxx mpiCC mpic++ hcp mpxlC_r mpxlC mpCC cmpic++ CC, $CXX)
	acx_mpi_save_CXX="$CXX"
	CXX="$MPICXX"
	AC_SUBST(MPICXX)
],
[Fortran 77], [
	AC_REQUIRE([AC_PROG_F77])
	AC_ARG_VAR(MPIF77,[MPI Fortran 77 compiler command])
	AC_CHECK_PROGS(MPIF77, mpif77 hf77 mpxlf mpf77 mpif90 mpf90 mpxlf90 mpxlf95 mpxlf_r cmpifc cmpif90c, $F77)
	acx_mpi_save_F77="$F77"
	F77="$MPIF77"
	AC_SUBST(MPIF77)
],
[Fortran], [
	AC_REQUIRE([AC_PROG_FC])
	AC_ARG_VAR(MPIFC,[MPI Fortran compiler command])
	AC_CHECK_PROGS(MPIFC, mpif90 hf90 mpxlf90 mpxlf95 mpf90 cmpifc cmpif90c, $FC)
	acx_mpi_save_FC="$FC"
	FC="$MPIFC"
	AC_SUBST(MPIFC)
])

if test x = x"$MPILIBS"; then
	AC_LANG_CASE([C], [AC_CHECK_FUNC(MPI_Init, [MPILIBS=" "])],
		[C++], [AC_CHECK_FUNC(MPI_Init, [MPILIBS=" "])],
		[Fortran 77], [AC_MSG_CHECKING([for MPI_Init])
			AC_LINK_IFELSE([AC_LANG_PROGRAM([],[      call MPI_Init])],[MPILIBS=" "
				AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)])],
		[Fortran], [AC_MSG_CHECKING([for MPI_Init])
			AC_LINK_IFELSE([AC_LANG_PROGRAM([],[      call MPI_Init])],[MPILIBS=" "
				AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)])])
fi
AC_LANG_CASE([Fortran 77], [
	if test x = x"$MPILIBS"; then
		AC_CHECK_LIB(fmpi, MPI_Init, [MPILIBS="-lfmpi"])
	fi
	if test x = x"$MPILIBS"; then
		AC_CHECK_LIB(fmpich, MPI_Init, [MPILIBS="-lfmpich"])
	fi
],
[Fortran], [
	if test x = x"$MPILIBS"; then
		AC_CHECK_LIB(fmpi, MPI_Init, [MPILIBS="-lfmpi"])
	fi
	if test x = x"$MPILIBS"; then
		AC_CHECK_LIB(mpichf90, MPI_Init, [MPILIBS="-lmpichf90"])
	fi
])
if test x = x"$MPILIBS"; then
	AC_CHECK_LIB(mpi, MPI_Init, [MPILIBS="-lmpi"])
fi
if test x = x"$MPILIBS"; then
	AC_CHECK_LIB(mpich, MPI_Init, [MPILIBS="-lmpich"])
fi

dnl We have to use AC_TRY_COMPILE and not AC_CHECK_HEADER because the
dnl latter uses $CPP, not $CC (which may be mpicc).
AC_LANG_CASE([C], [if test x != x"$MPILIBS"; then
	AC_MSG_CHECKING([for mpi.h])
	AC_TRY_COMPILE([#include &lt;mpi.h&gt;],[],[AC_MSG_RESULT(yes)], [MPILIBS=""
		AC_MSG_RESULT(no)])
fi],
[C++], [if test x != x"$MPILIBS"; then
	AC_MSG_CHECKING([for mpi.h])
	AC_TRY_COMPILE([#include &lt;mpi.h&gt;],[],[AC_MSG_RESULT(yes)], [MPILIBS=""
		AC_MSG_RESULT(no)])
fi],
[Fortran 77], [if test x != x"$MPILIBS"; then
	AC_MSG_CHECKING([for mpif.h])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[      include 'mpif.h'])],[AC_MSG_RESULT(yes)], [MPILIBS=""
		AC_MSG_RESULT(no)])
fi],
[Fortran], [if test x != x"$MPILIBS"; then
	AC_MSG_CHECKING([for mpif.h])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[      include 'mpif.h'])],[AC_MSG_RESULT(yes)], [MPILIBS=""
		AC_MSG_RESULT(no)])
fi])

AC_LANG_CASE([C], [CC="$acx_mpi_save_CC"],
	[C++], [CXX="$acx_mpi_save_CXX"],
	[Fortran 77], [F77="$acx_mpi_save_F77"],
	[Fortran], [FC="$acx_mpi_save_FC"])

AC_SUBST(MPILIBS)

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test x = x"$MPILIBS"; then
        $2
        :
else
        ifelse([$1],,[AC_DEFINE(HAVE_MPI,1,[Define if you have the MPI library.])],[$1])
        :
fi
])dnl ACX_MPI

AC_LANG([C])
ACX_MPI()
AC_LANG([C++])
ACX_MPI()

# Checks for libraries.
AC_CHECK_LIB([m], [sqrt])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([malloc.h stdlib.h string.h strings.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_HEADER_STDBOOL
#AC_C_CONST
#AC_TYPE_SIZE_T
#AC_C_VOLATILE

# Checks for library functions.
#AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_VPRINTF
#AC_CHECK_FUNCS([pow sqrt strcasecmp])

# Only make.config is altered by configure
AC_OUTPUT(make.config)
