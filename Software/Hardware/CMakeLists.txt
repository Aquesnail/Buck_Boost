# Collect all C source files in Hardware directory
file(GLOB ST7796_HW_DRIVER_SOURCES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

# Create Hardware interface library
add_library(Hardware INTERFACE)

target_sources(Hardware INTERFACE
    $<TARGET_OBJECTS:Hardware>
    FILE_SET HEADERS
    BASE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Export variable to parent scope for main CMakeLists.txt
set(ST7796_HW_DRIVER_SOURCES ${ST7796_HW_DRIVER_SOURCES} PARENT_SCOPE)