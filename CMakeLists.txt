cmake_minimum_required(VERSION 3.19)
project(Voltix LANGUAGES C CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(Voltix
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    core.h
    core.c
)

qt_add_translations(
    TARGETS Voltix
    TS_FILES Voltix_fr_FR.ts
)

target_link_libraries(Voltix
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS Voltix
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Voltix
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
