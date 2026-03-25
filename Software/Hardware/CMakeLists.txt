set(HW_SOURCES
    
    button.c
    font.c
   
    ST7789.c
)
# 2. 创建名为 Hardware 的静态库
add_library(Hardware STATIC ${HW_SOURCES})

target_link_libraries(Hardware PRIVATE stm32cubemx)

# 3. 指定头文件路径
# 使用 "." 表示当前目录，PUBLIC 确保链接 Hardware 的目标也能引用这里的 .h
target_include_directories(Hardware PUBLIC .)

# 原本我想把interrupt放在一个专门的.c文件里，但是这样的话没有人调用它们就会被链接器优化