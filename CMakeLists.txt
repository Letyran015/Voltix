cmake_minimum_required(VERSION 3.19)
project(Voltix LANGUAGES C CXX)

set(VOLTIX_VERSION "0.0.0-dev" CACHE STRING "Version de Voltix")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network LinguistTools)

qt_standard_project_setup()

qt_add_executable(Voltix
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    core.h
    core.c
    symbolitem.h
    symbolitem.cpp
    electro.cpp
    wireitem.h
    schemaview.h
    schemaview.cpp
    propertiesdialog.h
    plotwidget.h
    spice.h
    updatechecker.h
    catalog.h
    librarypanel.h
    welcomepage.h
    schemaview_sim.cpp
    electro2.cpp
    schemaview_sim.cpp
    project.h
    sheet.h
    wiredialog.h
    projectdialog.h
    project.h
    sheet.h
    wiredialog.h
    projectdialog.h
    electro2.cpp
    bench.h
    bench.cpp
)

# images facultatives des cartes d'accueil : resources/electronique.jpg, etc.
file(GLOB VOLTIX_IMAGES "${CMAKE_SOURCE_DIR}/resources/*.jpg" "${CMAKE_SOURCE_DIR}/resources/*.png")
if(VOLTIX_IMAGES)
    qt_add_resources(Voltix "images"
        PREFIX "/img"
        BASE "${CMAKE_SOURCE_DIR}/resources"
        FILES ${VOLTIX_IMAGES})
endif()

qt_add_translations(
    TARGETS Voltix
    TS_FILES Voltix_fr_FR.ts
)

target_link_libraries(Voltix
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

target_compile_definitions(Voltix PRIVATE VOLTIX_VERSION="${VOLTIX_VERSION}")

if(MSVC)
    target_compile_options(Voltix PRIVATE /utf-8)
    target_compile_definitions(Voltix PRIVATE _USE_MATH_DEFINES)
endif()

# --- ngspice embarqué (développement : copié à côté de l'exécutable) ---
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/ngspice)
    add_custom_command(TARGET Voltix POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/third_party/ngspice
                $<TARGET_FILE_DIR:Voltix>/ngspice)
endif()

# --- installation ---
include(GNUInstallDirs)

install(TARGETS Voltix
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(APPLE)
    set(VOLTIX_DATA_DIR Voltix.app/Contents/Resources)
else()
    set(VOLTIX_DATA_DIR ${CMAKE_INSTALL_BINDIR})
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/ngspice)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/ngspice/
            DESTINATION ${VOLTIX_DATA_DIR}/ngspice
            USE_SOURCE_PERMISSIONS)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/licenses)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/licenses/
            DESTINATION ${VOLTIX_DATA_DIR}/licenses)
endif()

if(WIN32 OR APPLE)
    qt_generate_deploy_app_script(
        TARGET Voltix
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()