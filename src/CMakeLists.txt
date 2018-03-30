# OpenCV依赖
find_package( OpenCV REQUIRED )

# OpenGL
find_package(OpenGL REQUIRED)

# GLUT
find_package(GLUT REQUIRED)

# Glew
find_package(Glew REQUIRED)

# 设置SiftGPU头文件与库文件所在位置
include_directories( ${PROJECT_BINARY_DIR}/SiftGPU ${OpenGL_INCLUDE_DIR})
set( SIFTGPU_LIBS ${PROJECT_BINARY_DIR}/bin/libsiftgpu.so )

add_library(fusion preprocess.cpp)
add_executable( material_fusion main.cpp)

target_link_libraries( material_fusion
    fusion
    ${OpenCV_LIBS}
    ${SIFTGPU_LIBS}
    ${GLEW_LIBRARIES} ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES}
)