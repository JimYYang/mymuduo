# 定义参与编译的源代码文件
aux_source_directory(. SRC_LIST)
# 编译生成动态库 netServer
add_library(netServer SHARED ${SRC_LIST})