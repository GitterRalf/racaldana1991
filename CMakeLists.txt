cmake_minimum_required(VERSION 3.21)
project(RacalDana1991 VERSION 1.0.0 LANGUAGES CXX RC)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent Network LinguistTools)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)
set(QT_I18N_SOURCE_LANGUAGE de)

set(QWT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/qwt" CACHE PATH "Qt-6-Qwt installation")
find_path(QWT_INCLUDE_DIR NAMES qwt_plot.h HINTS "${QWT_ROOT}" PATH_SUFFIXES include include/qwt REQUIRED)
find_library(QWT_IMPORT_LIBRARY NAMES qwt qwt-qt6 HINTS "${QWT_ROOT}" PATH_SUFFIXES lib REQUIRED)
find_file(QWT_RUNTIME_DLL NAMES qwt.dll qwt-qt6.dll HINTS "${QWT_ROOT}" PATH_SUFFIXES bin lib REQUIRED)
add_library(Qwt::Qwt SHARED IMPORTED)
set_target_properties(Qwt::Qwt PROPERTIES
    IMPORTED_IMPLIB "${QWT_IMPORT_LIBRARY}"
    IMPORTED_LOCATION "${QWT_RUNTIME_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${QWT_INCLUDE_DIR}"
    INTERFACE_COMPILE_DEFINITIONS QWT_DLL
)

qt_add_executable(RacalDana1991 WIN32
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
target_link_libraries(RacalDana1991 PRIVATE Qt6::Widgets Qt6::Concurrent Qt6::Network Qwt::Qwt)
if(MSVC)
    target_compile_options(RacalDana1991 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

add_custom_command(TARGET RacalDana1991 POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qwt::Qwt>" "$<TARGET_FILE_DIR:RacalDana1991>"
    VERBATIM
)

qt_add_translations(RacalDana1991 TS_FILES translations/racaldana_en.ts translations/racaldana_fr.ts)
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
