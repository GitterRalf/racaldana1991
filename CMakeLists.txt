cmake_minimum_required(VERSION 3.21)
project(RacalDana1991 VERSION 1.0.1 LANGUAGES CXX RC)

set(DEVICE_QT_MAJOR 6 CACHE STRING "Qt major version (5 or 6)")
set_property(CACHE DEVICE_QT_MAJOR PROPERTY STRINGS 5 6)
if(DEVICE_QT_MAJOR STREQUAL "5")
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Concurrent Network LinguistTools)
    set(CMAKE_AUTOMOC ON)
elseif(DEVICE_QT_MAJOR STREQUAL "6")
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent Network LinguistTools)
    qt_standard_project_setup()
else()
    message(FATAL_ERROR "DEVICE_QT_MAJOR must be 5 or 6.")
endif()
set(qt "Qt${DEVICE_QT_MAJOR}")
set(CMAKE_AUTORCC ON)
set(QT_I18N_SOURCE_LANGUAGE de)

if(DEVICE_QT_MAJOR STREQUAL "5")
    set(qwt_dir qwt-qt5-x86)
else()
    set(qwt_dir qwt)
endif()
set(QWT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/${qwt_dir}" CACHE PATH "Qwt built for the selected Qt kit")
find_path(QWT_INCLUDE_DIR NAMES qwt_plot.h HINTS "${QWT_ROOT}" PATH_SUFFIXES include include/qwt REQUIRED)
find_library(QWT_IMPORT_LIBRARY NAMES qwt qwt-qt${DEVICE_QT_MAJOR} HINTS "${QWT_ROOT}" PATH_SUFFIXES lib REQUIRED)
find_file(QWT_RUNTIME_DLL NAMES qwt.dll qwt-qt${DEVICE_QT_MAJOR}.dll HINTS "${QWT_ROOT}" PATH_SUFFIXES bin lib REQUIRED)
add_library(Qwt::Qwt SHARED IMPORTED)
set_target_properties(Qwt::Qwt PROPERTIES
    IMPORTED_IMPLIB "${QWT_IMPORT_LIBRARY}"
    IMPORTED_LOCATION "${QWT_RUNTIME_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${QWT_INCLUDE_DIR}"
    INTERFACE_COMPILE_DEFINITIONS QWT_DLL
)

if(DEVICE_QT_MAJOR STREQUAL "5")
    add_executable(RacalDana1991 WIN32)
else()
    qt_add_executable(RacalDana1991 WIN32)
endif()
target_sources(RacalDana1991 PRIVATE
    main.cpp
    mainwindow.cpp mainwindow.h measurementui.cpp frequencyplot.cpp updates.cpp
    racaldana.cpp racaldana.h
    gpib.cpp gpib.h
    assets/app.qrc assets/app.rc
)
target_compile_features(RacalDana1991 PRIVATE cxx_std_17)
target_compile_definitions(RacalDana1991 PRIVATE
    RACALDANA1991_VERSION="${PROJECT_VERSION}"
    NOMINMAX WIN32_LEAN_AND_MEAN
)
target_include_directories(RacalDana1991 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(RacalDana1991 PRIVATE ${qt}::Widgets ${qt}::Concurrent ${qt}::Network Qwt::Qwt)
if(MSVC)
    target_compile_options(RacalDana1991 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

add_custom_command(TARGET RacalDana1991 POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qwt::Qwt>" "$<TARGET_FILE_DIR:RacalDana1991>"
    VERBATIM
)

set(ts_files translations/racaldana_en.ts translations/racaldana_fr.ts)
if(DEVICE_QT_MAJOR STREQUAL "5")
    qt5_add_translation(qm_files ${ts_files})
    set(translation_resources "<RCC><qresource prefix=\"/i18n\">\n")
    foreach(qm IN LISTS qm_files)
        get_filename_component(filename "${qm}" NAME)
        string(APPEND translation_resources "  <file alias=\"${filename}\">${qm}</file>\n")
    endforeach()
    string(APPEND translation_resources "</qresource><qresource prefix=\"/qt-i18n\">\n")
    foreach(locale IN ITEMS de fr)
        get_filename_component(qt_catalog "${Qt5_DIR}/../../../translations/qtbase_${locale}.qm" ABSOLUTE)
        if(EXISTS "${qt_catalog}")
            string(APPEND translation_resources "  <file alias=\"qtbase_${locale}.qm\">${qt_catalog}</file>\n")
        endif()
    endforeach()
    string(APPEND translation_resources "</qresource></RCC>\n")
    set(translations_qrc "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")
    file(CONFIGURE OUTPUT "${translations_qrc}" CONTENT "${translation_resources}" @ONLY)
    qt5_add_resources(translation_sources "${translations_qrc}")
    target_sources(RacalDana1991 PRIVATE ${translation_sources})
else()
    qt_add_translations(RacalDana1991 TS_FILES ${ts_files})
    foreach(locale IN ITEMS de fr)
        set(qt_catalog "${Qt6_DIR}/../../../translations/qtbase_${locale}.qm")
        if(EXISTS "${qt_catalog}")
            get_filename_component(qt_catalog "${qt_catalog}" ABSOLUTE)
            set_source_files_properties("${qt_catalog}" PROPERTIES QT_RESOURCE_ALIAS "qtbase_${locale}.qm")
            qt_add_resources(RacalDana1991 "qt_i18n_${locale}"
                PREFIX "/qt-i18n"
                FILES "${qt_catalog}"
            )
        endif()
    endforeach()
endif()
