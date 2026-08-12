cmake_minimum_required(VERSION 3.22)

project(nekowall VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

# Network, JSON and image work all come from Qt itself, so the finished
# program needs nothing beyond the Qt libraries the desktop already has.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(nekowall
	src/main.cpp
	src/autostart.cpp
	src/filters.cpp
	src/source.cpp
	src/theme.cpp
	src/wallpaper.cpp
	src/window.cpp
)

target_compile_definitions(nekowall PRIVATE NEKOWALL_VERSION="${PROJECT_VERSION}")
target_link_libraries(nekowall PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

install(TARGETS nekowall RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/nekowall.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
# The user manager reads its units from below the prefix, never from lib64.
install(FILES
	data/nekowall.service
	data/nekowall-rotate.service
	data/nekowall-rotate.timer
	DESTINATION lib/systemd/user)
