cmake_minimum_required (VERSION 2.8.11)
project (raze)

set (CPACK_PACKAGE_VERSION_MAJOR "10")
set (CPACK_PACKAGE_VERSION_MINOR "0")
set (CPACK_PACKAGE_VERSION_PATCH "1")

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
else()
	set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib:\$ORIGIN/")
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

set (RAZE_GUI OFF CACHE BOOL "")
set (RAZE_TEST OFF CACHE BOOL "")
option(RAZE_ASAN_INT "Enable ASan+UBSan+Integer overflow" OFF)
option(RAZE_ASAN "Enable ASan+UBSan" OFF)
option(RAZE_SIMD_OPTIMIZATIONS "Enable CPU-specific SIMD optimizations (SSE/AVX or NEON, e.g.)" OFF)
set (BOOST_CUSTOM OFF CACHE BOOL "")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if (WIN32)
	set (PLATFORM_COMPILE_FLAGS "-DBOOST_SPIRIT_THREADSAFE -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DWIN32_LEAN_AND_MEAN -DMINIUPNP_STATICLIB")
	set (BLAKE2_IMPLEMENTATION "blake2/blake2b.c")
else (WIN32)
	set (PLATFORM_COMPILE_FLAGS "-DBOOST_SPIRIT_THREADSAFE -Werror=switch -fPIC")
	if (RAZE_ASAN)
		set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=address,undefined -DED25519_NO_INLINE_ASM")
	elseif (RAZE_ASAN_INT)
		set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=address,undefined,integer -DED25519_NO_INLINE_ASM")
	elseif (RAZE_TSAN)
		set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=thread -DED25519_NO_INLINE_ASM")
	endif()

	IF (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86(_64)?)$")
		if (RAZE_SIMD_OPTIMIZATIONS)
			set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -msse4")
		elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
			set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -msse2")
		endif()

		set (BLAKE2_IMPLEMENTATION "blake2/blake2b.c")
		if (ENABLE_AVX2)
			set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -mavx2 -mbmi -mbmi2")
			if (PERMUTE_WITH_GATHER)
				set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -DPERMUTE_WITH_GATHER")
			elseif (PERMUTE_WITH_SHUFFLES)
				set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -DPERMUTE_WITH_SHUFFLES")
			endif()
		endif()
	else()
		set (BLAKE2_IMPLEMENTATION "blake2/blake2b-ref.c")
	endif()
endif (WIN32)

if (WIN32)
	set (PLATFORM_C_FLAGS "/std=c11")
else (WIN32)
	set (PLATFORM_C_FLAGS "-std=c11")
endif (WIN32)

if (WIN32)
	set (PLATFORM_CXX_FLAGS "/bigobj")
else (WIN32)
	set (PLATFORM_CXX_FLAGS "-std=c++11")
endif (WIN32)

if (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES WIN32)
else (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES "")
endif (WIN32)

if (APPLE)
	set (PLATFORM_LINK_FLAGS "-framework Foundation -framework OpenCL")
elseif (WIN32)
	set (PLATFORM_LINK_FLAGS "")
else ()
	set (PLATFORM_LINK_FLAGS "-static-libgcc -static-libstdc++")
	if (RAZE_ASAN)
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined")
	elseif (RAZE_ASAN_INT)
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined,integer")
	elseif (RAZE_TSAN)
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=thread")
	endif()
endif (APPLE)

if (RAZE_GUI)
	if (WIN32)
		set (PLATFORM_QT_PACKAGES WinExtras)
	else (WIN32)
		set (PLATFORM_QT_PACKAGES)
	endif (WIN32)

	find_package (Qt5 COMPONENTS Core Gui Widgets Test ${PLATFORM_QT_PACKAGES})
	include_directories (${Qt5Core_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Test_INCLUDE_DIRS})
endif (RAZE_GUI)

include_directories (${CMAKE_SOURCE_DIR})

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)

if (BOOST_CUSTOM)
    list(APPEND CMAKE_MODULE_PATH ".\\" ${CMAKE_SOURCE_PATH})
endif (BOOST_CUSTOM)

find_package (Boost 1.66.0 REQUIRED COMPONENTS date_time filesystem system log log_setup thread program_options regex chrono atomic)
include_directories (${Boost_INCLUDE_DIR})

add_library (ed25519
	ed25519-donna/ed25519-donna-portable.h
	ed25519-donna/ed25519-hash-custom.h
	ed25519-donna/ed25519-randombytes-custom.h
	ed25519-donna/ed25519.h
	ed25519-donna/ed25519.c)

include_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2/include")
include_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2/src")
include_directories("${CMAKE_SOURCE_DIR}/blake2")

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set (ARGON_CORE phc-winner-argon2/src/opt.c)
else ()
	set (ARGON_CORE phc-winner-argon2/src/ref.c)
endif ()

add_subdirectory (gtest)
include_directories ("gtest/include")

include_directories (beast/include)

set (UPNPC_BUILD_SHARED OFF CACHE BOOL "")
add_subdirectory (miniupnp/miniupnpc)
include_directories (miniupnp/miniupnpc)

set (BUILD_SHARED OFF CACHE BOOL "")
set (BUILD_TESTING OFF CACHE BOOL "")
set (USE_INTERMEDIATE_OBJECTS_TARGET OFF CACHE BOOL "")
if (CRYPTOPP_CUSTOM OR NOT RAZE_SIMD_OPTIMIZATIONS)
	set (CRYPTOPP_LIBRARY cryptopp)
	add_library (cryptopp
	cryptopp/algparam.cpp
	cryptopp/asn.cpp
	cryptopp/basecode.cpp
	cryptopp/cpu.cpp
	cryptopp/cryptlib.cpp
	cryptopp/default.cpp
	cryptopp/des.cpp
	cryptopp/dessp.cpp
	cryptopp/dll.cpp
	cryptopp/ec2n.cpp
	cryptopp/ecp.cpp
	cryptopp/filters.cpp
	cryptopp/fips140.cpp
	cryptopp/gcm.cpp
	cryptopp/gf2n.cpp
	cryptopp/gfpcrypt.cpp
	cryptopp/hex.cpp
	cryptopp/hmac.cpp
	cryptopp/hrtimer.cpp
	cryptopp/integer.cpp
	cryptopp/iterhash.cpp
	cryptopp/misc.cpp
	cryptopp/modes.cpp
	cryptopp/mqueue.cpp
	cryptopp/nbtheory.cpp
	cryptopp/oaep.cpp
	cryptopp/osrng.cpp
	cryptopp/pubkey.cpp
	cryptopp/queue.cpp
	cryptopp/randpool.cpp
	cryptopp/rdtables.cpp
	cryptopp/rijndael.cpp
	cryptopp/rng.cpp
	cryptopp/sha.cpp
	cryptopp/simple.cpp
	cryptopp/winpipes.cpp)
	set_target_properties (cryptopp PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS}")
else ()
	set (CRYPTOPP_LIBRARY cryptopp-static)
	add_subdirectory (cryptopp)
endif ()

add_library (argon2
	phc-winner-argon2/src/argon2.c
	phc-winner-argon2/include/argon2.h
	phc-winner-argon2/src/core.c
	phc-winner-argon2/src/thread.c
	phc-winner-argon2/src/encoding.c
	${ARGON_CORE})

add_library (xxhash
	xxhash/xxhash.c
	xxhash/xxhash.h)

add_library (lmdb
	lmdb/libraries/liblmdb/lmdb.h
	lmdb/libraries/liblmdb/mdb.c
	lmdb/libraries/liblmdb/midl.c)

add_library (blake2
	blake2/blake2-config.h
	blake2/blake2-impl.h
	blake2/blake2.h
	${BLAKE2_IMPLEMENTATION})

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set (PLATFORM_LIB_SOURCE raze/plat/default/priority.cpp)
	set (PLATFORM_SECURE_SOURCE raze/plat/osx/working.mm)
	set (PLATFORM_WALLET_SOURCE raze/plat/default/icon.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set (PLATFORM_LIB_SOURCE raze/plat/windows/priority.cpp)
	set (PLATFORM_SECURE_SOURCE raze/plat/windows/working.cpp)
	set (PLATFORM_NODE_SOURCE raze/plat/windows/openclapi.cpp)
	set (PLATFORM_WALLET_SOURCE raze/plat/windows/icon.cpp Raze.rc)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set (PLATFORM_LIB_SOURCE raze/plat/linux/priority.cpp)
	set (PLATFORM_SECURE_SOURCE raze/plat/posix/working.cpp)
	set (PLATFORM_NODE_SOURCE raze/plat/posix/openclapi.cpp)
	set (PLATFORM_WALLET_SOURCE raze/plat/default/icon.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
	set (PLATFORM_LIB_SOURCE raze/plat/default/priority.cpp)
	set (PLATFORM_SECURE_SOURCE raze/plat/posix/working.cpp)
	set (PLATFORM_NODE_SOURCE raze/plat/posix/openclapi.cpp)
	set (PLATFORM_WALLET_SOURCE raze/plat/default/icon.cpp)
else ()
	error ("Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif ()

# Embed bootstrap representative weights in executable
file (READ rep_weights.bin filedata HEX)
string (REGEX REPLACE "(..)" "0x\\1," filedata ${filedata})
file (WRITE ${CMAKE_BINARY_DIR}/bootstrap_weights.cpp "#include <cstddef>\n"
	"namespace raze {\n"
	"	unsigned char raze_bootstrap_weights[] = {${filedata} 0x00};\n"
	"	size_t raze_bootstrap_weights_size = sizeof(raze_bootstrap_weights) - 1;\n"
	"}\n")

add_library (secure
	${PLATFORM_SECURE_SOURCE}
	${CMAKE_BINARY_DIR}/bootstrap_weights.cpp
	raze/config.hpp
	raze/secure.cpp
	raze/secure.hpp
	raze/node/utility.cpp
	raze/node/utility.hpp
	raze/versioning.hpp
	raze/versioning.cpp)

SET (RAZE_LIB_SOURCES
	${PLATFORM_LIB_SOURCE}
	raze/lib/blocks.cpp
	raze/lib/blocks.hpp
	raze/lib/interface.cpp
	raze/lib/interface.h
	raze/lib/numbers.cpp
	raze/lib/numbers.hpp
	raze/lib/utility.cpp
	raze/lib/utility.hpp
	raze/lib/work.hpp
	raze/lib/work.cpp)

add_library (raze_lib SHARED ${RAZE_LIB_SOURCES})
add_library (raze_lib_static STATIC ${RAZE_LIB_SOURCES})

add_library (node
	${PLATFORM_NODE_SOURCE}
	raze/node/bootstrap.cpp
	raze/node/bootstrap.hpp
	raze/node/common.cpp
	raze/node/common.hpp
	raze/node/node.hpp
	raze/node/node.cpp
	raze/node/openclwork.cpp
	raze/node/openclwork.hpp
	raze/node/rpc.hpp
	raze/node/rpc.cpp
	raze/node/testing.hpp
	raze/node/testing.cpp
	raze/node/wallet.hpp
	raze/node/wallet.cpp
	raze/node/working.hpp
	raze/node/xorshift.hpp)


SET (ACTIVE_NETWORK raze_live_network CACHE STRING "Selects which network parameters are used")
set_property (CACHE ACTIVE_NETWORK PROPERTY STRINGS raze_test_network raze_beta_network raze_live_network)

if (RAZE_TEST)
	add_executable (core_test
		raze/core_test/block.cpp
		raze/core_test/block_store.cpp
		raze/core_test/interface.cpp
		raze/core_test/conflicts.cpp
		raze/core_test/daemon.cpp
		raze/core_test/entry.cpp
		raze/core_test/gap_cache.cpp
		raze/core_test/ledger.cpp
		raze/core_test/network.cpp
		raze/core_test/node.cpp
		raze/core_test/message.cpp
		raze/core_test/message_parser.cpp
		raze/core_test/processor_service.cpp
		raze/core_test/peer_container.cpp
		raze/core_test/rpc.cpp
		raze/core_test/uint256_union.cpp
		raze/core_test/versioning.cpp
		raze/core_test/wallet.cpp
		raze/core_test/wallets.cpp
		raze/core_test/work_pool.cpp)

	add_executable (slow_test
		raze/slow_test/node.cpp)
	
	set_target_properties (core_test slow_test PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DRAZE_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DRAZE_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
	set_target_properties (core_test slow_test PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
endif (RAZE_TEST)

if (RAZE_GUI)

	qt5_add_resources(RES resources.qrc)


	add_library (qt
		raze/qt/qt.cpp
		raze/qt/qt.hpp)

	add_executable (nano_wallet ${PLATFORM_GUI_TARGET_PROPERTIES}
		${PLATFORM_WALLET_SOURCE}
		raze/raze_wallet/entry.cpp
		${RES})

	add_executable (qt_test
		raze/qt_test/entry.cpp
		raze/qt_test/qt.cpp)

	add_executable (qt_system
		raze/qt_system/entry.cpp)

	set_target_properties (nano_wallet qt_test PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
	
	set_target_properties (qt_test qt nano_wallet qt_system PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DRAIBLOCKS_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DRAIBLOCKS_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
	set_target_properties (qt qt_system PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
endif (RAZE_GUI)

add_executable (raze_node
	raze/raze_node/daemon.cpp
	raze/raze_node/daemon.hpp
	raze/raze_node/entry.cpp)

set_target_properties (argon2 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS}")
set_target_properties (blake2 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS} -D__SSE2__")
set_target_properties (ed25519 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DED25519_CUSTOMHASH -DED25519_CUSTOMRNG")
set_target_properties (secure node raze_node raze_lib raze_lib_static PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DRAZE_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DRAZE_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
set_target_properties (secure node raze_node PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")

if (WIN32)
	set (PLATFORM_LIBS Ws2_32 mswsock iphlpapi ntdll)
else (WIN32)
	set (PLATFORM_LIBS pthread ${CMAKE_DL_LIBS})
endif (WIN32)

if (WIN32)
	set (PLATFORM_WALLET_LIBS Qt5::WinExtras)
else (WIN32)
	set (PLATFORM_WALLET_LIBS)
endif (WIN32)

if (RAZE_TEST)
	target_link_libraries (core_test node secure lmdb ed25519 raze_lib_static argon2 ${CRYPTOPP_LIBRARY} gtest_main gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})

	target_link_libraries (slow_test node secure lmdb ed25519 raze_lib_static argon2 ${CRYPTOPP_LIBRARY} gtest_main gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})
endif (RAZE_TEST)

if (RAZE_GUI)
	target_link_libraries (qt_test node secure lmdb ed25519 raze_lib_static qt argon2 ${CRYPTOPP_LIBRARY} gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Test ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS})

	target_link_libraries (qt_system node secure lmdb ed25519 raze_lib_static qt argon2 ${CRYPTOPP_LIBRARY} gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS})

	target_link_libraries (nano_wallet node secure lmdb ed25519 raze_lib_static qt argon2 ${CRYPTOPP_LIBRARY} libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS} ${PLATFORM_WALLET_LIBS})
endif (RAIBLOCKS_GUI)

target_link_libraries (raze_lib ed25519 xxhash blake2 ${CRYPTOPP_LIBRARY})
target_link_libraries (raze_lib_static ed25519 xxhash blake2 ${CRYPTOPP_LIBRARY})

target_link_libraries (raze_node node secure lmdb ed25519 raze_lib_static argon2 ${CRYPTOPP_LIBRARY} libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})

set (CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
if (RAZE_GUI)
	if (APPLE)
		get_filename_component (Qt5Core_framework_DIR ${Qt5_DIR}/../../QtCore.framework ABSOLUTE)
		get_filename_component (Qt5Gui_framework_DIR ${Qt5_DIR}/../../QtGui.framework ABSOLUTE)
		get_filename_component (Qt5PrintSupport_framework_DIR ${Qt5_DIR}/../../QtPrintSupport.framework ABSOLUTE)
		get_filename_component (Qt5Test_framework_DIR ${Qt5_DIR}/../../QtTest.framework ABSOLUTE)
		get_filename_component (Qt5Widgets_framework_DIR ${Qt5_DIR}/../../QtWidgets.framework ABSOLUTE)
		get_filename_component (Qt5_platforms_DIR ${Qt5_DIR}/../../../plugins/platforms ABSOLUTE)

		file (COPY ${Qt5Core_framework_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/packaging)
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-id" "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtCore.framework/Versions/5/QtCore")

		file (COPY ${Qt5Gui_framework_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/packaging)
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-id" "@executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtGui.framework/Versions/5/QtGui")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Core_framework_DIR}/Versions/5/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtGui.framework/Versions/5/QtGui")

		file (COPY ${Qt5PrintSupport_framework_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/packaging)
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-id" "@executable_path/../Frameworks/QtPrintSupport.framework/Versions/5/QtPrintSupport" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtPrintSupport.framework/Versions/5/QtPrintSupport")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Core_framework_DIR}/Versions/5/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtPrintSupport.framework/Versions/5/QtPrintSupport")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Gui_framework_DIR}/Versions/5/QtGui" "@executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtPrintSupport.framework/Versions/5/QtPrintSupport")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Widgets_framework_DIR}/Versions/5/QtWidgets" "@executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtPrintSupport.framework/Versions/5/QtPrintSupport")

		file (COPY ${Qt5Test_framework_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/packaging)
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-id" "@executable_path/../Frameworks/QtTest.framework/Versions/5/QtTest" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtTest.framework/Versions/5/QtTest")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Core_framework_DIR}/Versions/5/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtTest.framework/Versions/5/QtTest")

		file (COPY ${Qt5Widgets_framework_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/packaging)
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-id" "@executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtWidgets.framework/Versions/5/QtWidgets")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Core_framework_DIR}/Versions/5/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtWidgets.framework/Versions/5/QtWidgets")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Gui_framework_DIR}/Versions/5/QtGui" "@executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui" "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtWidgets.framework/Versions/5/QtWidgets")

		file (COPY ${Qt5_DIR}/../../../plugins/platforms/libqcocoa.dylib DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/packaging)
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Core_framework_DIR}/Versions/5/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore" "${CMAKE_CURRENT_BINARY_DIR}/packaging/libqcocoa.dylib")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Gui_framework_DIR}/Versions/5/QtGui" "@executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui" "${CMAKE_CURRENT_BINARY_DIR}/packaging/libqcocoa.dylib")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5PrintSupport_framework_DIR}/Versions/5/QtPrintSupport" "@executable_path/../Frameworks/QtPrintSupport.framework/Versions/5/QtPrintSupport" "${CMAKE_CURRENT_BINARY_DIR}/packaging/libqcocoa.dylib")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Widgets_framework_DIR}/Versions/5/QtWidgets" "@executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets" "${CMAKE_CURRENT_BINARY_DIR}/packaging/libqcocoa.dylib")

		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nano_wallet> ${CMAKE_CURRENT_BINARY_DIR}/packaging)
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Core_framework_DIR}/Versions/5/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore" "${CMAKE_CURRENT_BINARY_DIR}/packaging/nano_wallet")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Gui_framework_DIR}/Versions/5/QtGui" "@executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui" "${CMAKE_CURRENT_BINARY_DIR}/packaging/nano_wallet")
		add_custom_command (TARGET nano_wallet POST_BUILD COMMAND "install_name_tool" "-change" "${Qt5Widgets_framework_DIR}/Versions/5/QtWidgets" "@executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets" "${CMAKE_CURRENT_BINARY_DIR}/packaging/nano_wallet")

		install (FILES ${CMAKE_CURRENT_BINARY_DIR}/packaging/nano_wallet DESTINATION Nano.app/Contents/MacOS PERMISSIONS OWNER_EXECUTE OWNER_READ)
		install (FILES Info.plist DESTINATION Nano.app/Contents)
		install (FILES qt.conf DESTINATION Nano.app/Contents/Resources)
		install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtCore.framework" DESTINATION Nano.app/Contents/Frameworks USE_SOURCE_PERMISSIONS)
		install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtGui.framework" DESTINATION Nano.app/Contents/Frameworks USE_SOURCE_PERMISSIONS)
		install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtPrintSupport.framework" DESTINATION Nano.app/Contents/Frameworks USE_SOURCE_PERMISSIONS)
		install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtTest.framework" DESTINATION Nano.app/Contents/Frameworks USE_SOURCE_PERMISSIONS)
		install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packaging/QtWidgets.framework" DESTINATION Nano.app/Contents/Frameworks USE_SOURCE_PERMISSIONS)
		install (FILES "${CMAKE_CURRENT_BINARY_DIR}/packaging/libqcocoa.dylib" DESTINATION Nano.app/Contents/PlugIns/platforms PERMISSIONS OWNER_EXECUTE OWNER_READ)
		install (FILES RaiBlocks.icns DESTINATION Nano.app/Contents/Resources)
	elseif (WIN32)
		if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
			set (WIN_REDIST vc_redist.x64.exe)
		else ()
			set (WIN_REDIST vc_redist.x86.exe)
		endif ()
		set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ExecWait '\\\"$INSTDIR\\\\${WIN_REDIST}\\\" /quiet /norestart'")
		set (CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/RaiBlocks.ico)
		set (CPACK_NSIS_DISPLAY_NAME "Nano")
		set (CPACK_NSIS_PACKAGE_NAME "Nano ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
		set (CPACK_NSIS_URL_INFO_ABOUT "https://nano.org")
		set (CPACK_NSIS_CONTACT "clemahieu@raiblocks.net")
		set (CPACK_NSIS_MENU_LINKS "nano_wallet.exe" "Nano Wallet" "https://nano.org" "Nano website")
		set (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
		get_target_property (Qt5WindowsPlugin Qt5::QWindowsIntegrationPlugin LOCATION)
		get_filename_component (Qt5_bin_DIR ${Qt5_DIR}/../../../bin ABSOLUTE)
		install (TARGETS nano_wallet DESTINATION .)
		install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${WIN_REDIST} DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/libGLESv2.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Core.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Gui.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Widgets.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5WinExtras.dll DESTINATION .)
		install (FILES ${Qt5WindowsPlugin} DESTINATION platforms)
	else ()
		install(TARGETS nano_wallet raze_node raze_lib
				RUNTIME DESTINATION bin
				LIBRARY DESTINATION lib
				ARCHIVE DESTINATION lib
		)
	endif ()
endif (RAZE_GUI)

include (CPack)
