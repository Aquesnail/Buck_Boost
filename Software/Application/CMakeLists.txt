# 1. 定义源文件列表
set(APP_SOURCES
    button_port.c
    ST7789_SPI_Port.c
    control.c
    power_ui_api.c
    ui_main.c
)

# 2. 创建一个名为 Application 的静态库
add_library(Application STATIC ${APP_SOURCES})

target_link_libraries(Application PRIVATE Hardware)
target_link_libraries(Application PRIVATE stm32cubemx)
# 3. 指定该库所需的头文件路径
# PUBLIC 表示任何链接到 Application 的目标都会自动获得这个头文件路径
target_include_directories(Application PUBLIC .)