cmake_minimum_required(VERSION 3.19)
project(Lza LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ALXLIB_INC ${CMAKE_SOURCE_DIR}/../../include)
set(ALXLIB_INC_BASE ${CMAKE_SOURCE_DIR}/../../include/alxbase)
set(ALXLIB_INC_CORE ${CMAKE_SOURCE_DIR}/../../include/alxcore)

# AUTORCC
set(CMAKE_AUTORCC ON)

qt_add_executable(Lza
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/Alexis_Lza.cpp
    src/Alexis_Lza.h
    src/Alexis_Lza.ui
    src/DecodeWidget.cpp
    src/DecodeWidget.h
    src/DecodeWidget.ui
    src/EncodeWidget.cpp
    src/EncodeWidget.h
    src/EncodeWidget.ui
    src/FSysTree.cpp
    src/FSysTree.h
    src/Global.h
    src/res/Alexis_Lza.qrc
)

target_include_directories(Lza PRIVATE
    src
    ${ALXLIB_INC}
    ${ALXLIB_INC_BASE}
    ${ALXLIB_INC_CORE}
)

target_link_libraries(Lza
    PRIVATE
        Qt::Core
        Qt::Widgets
        alxcore
)

install(TARGETS Lza
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
