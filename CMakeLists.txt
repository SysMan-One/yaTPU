#+++
#	DESCRIPTION: TPU - a lightweight text editor in the spirit of DEC TPU/EVE
#
#	AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE:  11-SEP-2026
#
#	MODIFICATION HISTORY:
#
#	11-SEP-2026	RRL	X-00-01 : Initial revision.
#	28-SEP-2026	RRL	X-00-02 : Header brought to the StarLet coding rules.
#	28-SEP-2026	RRL	X-00-06 : keycode utility, second target.
#	29-SEP-2026	RRL	X-00-09 : install rules: programs to bin, README, guides and KEYS.txt to share/doc/tpu.
#	12-NOV-2026	RRL	X-00-14 : The identification is read from tpu.c (__IDENT__, either form X.aa-bb[ECOcc] or
#				X-aa-bb[ECOc]), __REV__ checked against it; CPack makes the Debian package
#				yatpu_<aa.bb.cc>_<arch>.deb (prefix /usr, section editors) for .github/workflows/release.yml.
#
#	Builds standalone (cmake -S TPU -B build-tpu) or as a part of the WorkBench tree.
#	Needs the StarLet package installed (cmake --install of utility_routines, /usr/local by default).
#
#	The Debian package:	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && (cd build && cpack -G DEB)
#---
cmake_minimum_required(VERSION 3.16)
project(tpu LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# The identification is written once, in the head module tpu.c, and read back here.  __IDENT__ is X.aa-bb[ECOcc] (the
# form from 12-NOV-2026, two digits in every field) or the older X-aa-bb[ECOc]; the version aa.bb.cc of the package
# has no leading zeros, an absent ECO giving 0: X-00-14 -> 0.14.0.  The capture groups are taken at once - a further
# if() resets them.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tpu.c TPU_K_IDLINE REGEX "^#define[ \t]+__IDENT__")
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tpu.c TPU_K_REVLINE REGEX "^#define[ \t]+__REV__")
if ( NOT TPU_K_IDLINE MATCHES "\"([^\"]+)\"" )
	message(FATAL_ERROR "No __IDENT__ in tpu.c")
endif()
set(TPU_K_IDENT "${CMAKE_MATCH_1}")
if ( NOT TPU_K_REVLINE MATCHES "\"([^\"]+)\"" )
	message(FATAL_ERROR "No __REV__ in tpu.c")
endif()
set(TPU_K_REV "${CMAKE_MATCH_1}")

if ( TPU_K_IDENT MATCHES "^[A-Za-z]\\.([0-9][0-9])-([0-9][0-9])(ECO([0-9][0-9]))?$" )
elseif ( TPU_K_IDENT MATCHES "^[A-Za-z]-([0-9]+)-([0-9]+)(ECO([0-9]+))?$" )
else()
	message(FATAL_ERROR "__IDENT__ of tpu.c is '${TPU_K_IDENT}', of neither form X.aa-bb[ECOcc] nor X-aa-bb[ECOc]")
endif()
set(TPU_K_VMAJOR "${CMAKE_MATCH_1}")
set(TPU_K_VMINOR "${CMAKE_MATCH_2}")
set(TPU_K_VPATCH "${CMAKE_MATCH_4}")
if ( NOT TPU_K_VPATCH )
	set(TPU_K_VPATCH "0")
endif()
foreach(c VMAJOR VMINOR VPATCH)
	string(REGEX REPLACE "^0+([0-9])" "\\1" TPU_K_${c} "${TPU_K_${c}}")
endforeach()
set(TPU_K_DERIVED "${TPU_K_VMAJOR}.${TPU_K_VMINOR}.${TPU_K_VPATCH}")

# __REV__ is the same value in the UNIX style; tpu.c writes it with leading zeros (00.14.00), so it is compared field
# by field with them stripped
string(REGEX REPLACE "(^|\\.)0+([0-9])" "\\1\\2" TPU_K_REVNZ "${TPU_K_REV}")
if ( NOT TPU_K_REVNZ STREQUAL TPU_K_DERIVED )
	message(FATAL_ERROR "__REV__ of tpu.c is '${TPU_K_REV}', but __IDENT__ '${TPU_K_IDENT}' yields '${TPU_K_DERIVED}'")
endif()
message(STATUS "yaTPU identification ${TPU_K_IDENT}, revision ${TPU_K_REV}, package version ${TPU_K_DERIVED}")

# StarLet comes from the installed package (StarLet::starlet, /usr/local by default)
find_package(StarLet 1.6.3 REQUIRED)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)			# Standalone build; in the WorkBench tree the root defines these
	add_definitions( -D__ARCH__NAME__="${CMAKE_HOST_SYSTEM_PROCESSOR}" )
	if (CMAKE_BUILD_TYPE)
		string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)
	endif()
	if (build_type STREQUAL debug)
		add_definitions( -D_DEBUG=1 -D__TRACE__=1 )
	else()
		add_definitions( -DNDEBUG=1 )			# StarLet's $TRACE compiles away
	endif()
endif()

# Help text: tpu.hlp (VMS .HLP layout, "1 TOPIC" headers) is embedded into the binary
add_custom_command(
	OUTPUT	${CMAKE_CURRENT_BINARY_DIR}/tpu_help.h
	COMMAND	${CMAKE_COMMAND} -DIN=${CMAKE_CURRENT_SOURCE_DIR}/tpu.hlp -DOUT=${CMAKE_CURRENT_BINARY_DIR}/tpu_help.h
		-P ${CMAKE_CURRENT_SOURCE_DIR}/hlp2h.cmake
	DEPENDS	${CMAKE_CURRENT_SOURCE_DIR}/tpu.hlp ${CMAKE_CURRENT_SOURCE_DIR}/hlp2h.cmake
	COMMENT	"Embedding tpu.hlp"
)

add_executable(tpu tpu.c tpumsg.h tpukeys.h ${CMAKE_CURRENT_BINARY_DIR}/tpu_help.h)
add_executable(keycode keycode.c tpukeys.h)

target_include_directories(tpu PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
foreach(t tpu keycode)
	target_compile_options(${t} PRIVATE -Wall -Wno-format -Wno-pointer-sign -Wno-unused-result -Wno-unused-parameter -Wno-unused-function)
	target_link_libraries(${t} StarLet::starlet)
endforeach()

# Install: the two programs and the user documentation (cmake --install build [--prefix /opt/tpu])
include(GNUInstallDirs)
install(TARGETS tpu keycode RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md GUIDE-EN.md GUIDE-RU.md KEYS.txt DESTINATION ${CMAKE_INSTALL_DOCDIR})

# The Debian package yatpu_<aa.bb.cc>_<arch>.deb of what the install rules put down, always laid out under /usr (the
# help is embedded, nothing in the programs depends on the prefix).  StarLet is a static archive: the package needs
# the C library only, which dpkg-shlibdeps names.
if ( CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME )			# Standalone only: in the WorkBench tree the root packs
	set(CPACK_PACKAGE_NAME			"yatpu")
	set(CPACK_PACKAGE_VENDOR		"StarLet Squad")
	set(CPACK_PACKAGE_CONTACT		"StarLet Squad and Ruslan R. Laishev <polylab240@gmail.com>")
	set(CPACK_PACKAGE_VERSION_MAJOR		"${TPU_K_VMAJOR}")
	set(CPACK_PACKAGE_VERSION_MINOR		"${TPU_K_VMINOR}")
	set(CPACK_PACKAGE_VERSION_PATCH		"${TPU_K_VPATCH}")
	set(CPACK_PACKAGE_VERSION		"${TPU_K_DERIVED}")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY	"lightweight text editor in the spirit of DEC TPU/EVE and EDT")
	string(CONCAT CPACK_PACKAGE_DESCRIPTION
		"yaTPU is a text editor in the spirit of DEC TPU/EVE with the EDT keypad: buffers, windows, LEARN, "
		"key files, journal and -recover, on-line help; plain C and VT100 sequences, no ncurses.  "
		"keycode shows what a key sends and its TPU name.  Identification ${TPU_K_IDENT}.")
	set(CPACK_RESOURCE_FILE_LICENSE		"${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
	set(CPACK_PACKAGING_INSTALL_PREFIX	"/usr")
	set(CPACK_STRIP_FILES			ON)
	set(CPACK_VERBATIM_VARIABLES		ON)
	set(CPACK_GENERATOR			"DEB")

	set(CPACK_DEBIAN_FILE_NAME		DEB-DEFAULT)
	set(CPACK_DEBIAN_PACKAGE_SECTION	editors)
	set(CPACK_DEBIAN_PACKAGE_PRIORITY	optional)
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS	ON)
	set(CPACK_DEBIAN_PACKAGE_HOMEPAGE	"https://github.com/SysMan-One/yaTPU")

	include(CPack)
endif()
