# SIYI UniRC 7 / 7 Pro integration.
#
# Deliberately Qt-free: plain C++14 + POSIX (termios for the RC serial links,
# BSD sockets for the FPV upgrade link). That is the whole point of the port —
# it has to run on the UniRC 7 Pro's Android, where QSerialPort is unavailable.
# Keeping Qt out of this library also means it can be unit-tested and driven
# from a plain main() (see FIRMWARE_UPGRADE_PROTOCOL.md).
#
# POSIX-only, so this target is not built on Windows — see QGC_SIYI_ENABLED in
# the top-level CMakeLists.txt.

set(SOURCES
	fpv_upgrade_client.cpp
	ftp_upload.cpp
	rcu_session.cpp
	serial_port.cpp
	udp_port.cpp
	unirc_sdk.cpp
)

set(HEADERS
	crc.h
	fpv_frame.h
	fpv_upgrade_client.h
	ftp_upload.h
	rcu_session.h
	serial_port.h
	udp_port.h
	unirc_frame.h
	unirc_sdk.h
)

add_library(Siyi ${SOURCES} ${HEADERS})

# RcuSession / UniRcSdk each own I/O threads, and the FPV client sleeps between
# retries, so std::thread needs pthreads on Linux/Android.
find_package(Threads REQUIRED)

# No Q_OBJECT in this library, so skip Qt's autogen passes entirely (QGC turns
# AUTOMOC on globally; without this it generates an empty mocs_compilation.cpp).
set_target_properties(Siyi PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

target_link_libraries(Siyi PUBLIC Threads::Threads)
target_include_directories(Siyi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(Siyi PUBLIC cxx_std_14)
