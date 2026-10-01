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
#
#	Builds standalone (cmake -S TPU -B build-tpu) or as a part of the WorkBench tree.
#	Needs the StarLet package installed (cmake --install of utility_routines, /usr/local by default).
#---
cmake_minimum_required(VERSION 3.16)
project(tpu LANGUAGES C)

set(CMAKE_C_STANDARD 11)

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
