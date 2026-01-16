project(GPClient)

set(gpclient_GENERATED_SOURCES)

configure_file(com.qt.gpclient.desktop.in com.qt.gpclient.desktop)
configure_file(com.qt.gpclient.metainfo.xml.in com.qt.gpclient.metainfo.xml)

qt6_add_dbus_interface(
    gpclient_GENERATED_SOURCES
    ${CMAKE_BINARY_DIR}/com.qt.GPService.xml
    gpserviceinterface
)

add_executable(gpclient
    cdpcommand.cpp
    cdpcommandmanager.cpp
    enhancedwebview.cpp
    gatewayauthenticator.cpp
    gatewayauthenticatorparams.cpp
    gpgateway.cpp
    gphelper.cpp
    loginparams.cpp
    main.cpp
    standardloginwindow.cpp
    portalauthenticator.cpp
    portalconfigresponse.cpp
    preloginresponse.cpp
    samlloginwindow.cpp
    gpclient.cpp
    connectionmanager.cpp
    authenticationmanager.cpp
    systemtraymanager.cpp
    settingsmanager.cpp
    gpclient.ui
    standardloginwindow.ui
    challengedialog.h
    challengedialog.cpp
    challengedialog.ui
    vpn_dbus.cpp
    vpn_json.cpp
    # Qt6-native signal handler and single instance (headers for AUTOMOC)
    singleinstance.h
    signalhandler.h
    resources.qrc
    ${gpclient_GENERATED_SOURCES}
)

target_include_directories(gpclient PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${QTKEYCHAIN_INCLUDE_DIRS}/qt6keychain
)

target_link_libraries(gpclient
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
    Qt6::DBus
    Qt6::StateMachine
    ${QTKEYCHAIN_LIBRARIES}
)

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0 AND CMAKE_BUILD_TYPE STREQUAL Release)
    target_compile_options(gpclient PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
endif()


install(TARGETS gpclient DESTINATION bin)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/com.qt.gpclient.metainfo.xml" DESTINATION share/metainfo)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/com.qt.gpclient.desktop" DESTINATION share/applications)
install(FILES "com.qt.gpclient.svg" DESTINATION share/icons/hicolor/scalable/apps)
