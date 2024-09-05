cmake_minimum_required(VERSION 3.15)
set(CMAKE_GENERATOR_PLATFORM x64)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

project(sample_dx11 VERSION 1.0.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(default_icon_src 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceFiles/GPUOpenChip.ico
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceFiles/resource.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceFiles/sample_dx11.rc)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceFiles/sample_dx11.rc PROPERTIES VS_TOOL_OVERRIDE "Resource compiler")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceFiles/GPUOpenChip.ico  PROPERTIES VS_TOOL_OVERRIDE "Image")

add_compile_definitions(_UNICODE)
add_compile_definitions(UNICODE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceFiles ${CMAKE_CURRENT_SOURCE_DIR}/DXUT/Core ${CMAKE_CURRENT_SOURCE_DIR}/DXUT/Optional )

set(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Sample.cpp)
set(AL_PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/../ffx_antilag2_dx11.h)

file( GLOB DXUT_CORE
    "${CMAKE_CURRENT_SOURCE_DIR}/DXUT/Core/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/DXUT/Core/*.cpp"
)

file( GLOB DXUT_OPTIONAL
    "${CMAKE_CURRENT_SOURCE_DIR}/DXUT/Optional/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/DXUT/Optional/*.cpp"
)

add_executable(${PROJECT_NAME} WIN32 ${default_icon_src} ${SOURCES} ${AL_PUBLIC_HEADER} ${DXUT_CORE} ${DXUT_OPTIONAL})
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC D3D11 d3dcompiler comctl32 Imm32 Version winmm Usp10 Shlwapi)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX d)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

source_group("Source"                           FILES ${SOURCES}) 
source_group("Inc"                              FILES ${AL_PUBLIC_HEADER})
source_group("DXUT Core"                        FILES ${DXUT_CORE})
source_group("DXUT Optional"                    FILES ${DXUT_OPTIONAL})
source_group("Icon"    							FILES ${default_icon_src})

