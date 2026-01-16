project(GPService)

set(gpservice_GENERATED_SOURCES)

execute_process(COMMAND logname OUTPUT_VARIABLE CMAKE_LOGNAME)
string(STRIP "${CMAKE_LOGNAME}" CMAKE_LOGNAME)

message(STATUS "CMAKE_LOGNAME: ${CMAKE_LOGNAME}")

configure_file(dbus/com.qt.GPService.conf.in dbus/com.qt.GPService.conf)
configure_file(dbus/com.qt.GPService.service.in dbus/com.qt.GPService.service)
configure_file(systemd/gpservice.service.in systemd/gpservice.service)

# generate the dbus xml definition
qt6_generate_dbus_interface(
    gpservice.h
    ${CMAKE_BINARY_DIR}/com.qt.GPService.xml
)

# generate dbus adaptor
qt6_add_dbus_adaptor(
    gpservice_GENERATED_SOURCES
    ${CMAKE_BINARY_DIR}/com.qt.GPService.xml
    gpservice.h
    GPService
)

add_executable(gpservice
    gpservice.h
    gpservice.cpp
    main.cpp
    ${gpservice_GENERATED_SOURCES}
)

target_include_directories(gpservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(gpservice
    Qt6::Core
    Qt6::Network
    Qt6::DBus
)

install(TARGETS gpservice DESTINATION bin)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dbus/com.qt.GPService.conf" DESTINATION share/dbus-1/system.d )
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dbus/com.qt.GPService.service" DESTINATION share/dbus-1/system-services)
install(FILES "gp.conf" DESTINATION /etc/gpservice)

if("$ENV{DEBIAN_PACKAGE}")
    # Install the systemd unit files to /lib/systemd/system for debian package
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/systemd/gpservice.service" DESTINATION /lib/systemd/system)
else()
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/systemd/gpservice.service" DESTINATION lib/systemd/system)
endif()
