cmake_minimum_required(VERSION 3.21)
project(KeterAIStudio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Fix for macOS SDKs where AGL.framework was removed
if(APPLE)
    if(TARGET WrapOpenGL::WrapOpenGL)
        set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES
            INTERFACE_LINK_LIBRARIES "-framework OpenGL"
        )
    endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(PROJECT_SOURCES
    src/main.cpp
    include/IKeterPlugin.h
    include/HardwareChecker.h
    include/McpServer.h
    include/KeterMainWindow.h
)

if(WIN32)
    add_executable(KeterAIStudio WIN32 ${PROJECT_SOURCES})
elseif(APPLE)
    add_executable(KeterAIStudio MACOSX_BUNDLE ${PROJECT_SOURCES})
else()
    add_executable(KeterAIStudio ${PROJECT_SOURCES})
endif()

target_link_libraries(KeterAIStudio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)
