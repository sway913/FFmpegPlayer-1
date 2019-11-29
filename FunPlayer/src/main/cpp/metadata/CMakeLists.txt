# 添加头文件路径
include_directories(source)
# 添加源文件
add_library(metadata_retriever

        SHARED

        # library
        MediaMetadataRetriever.cpp
        Metadata.cpp
        MediaMetadataRetrieverEx_Jni.cpp)

# 链接静态库
target_link_libraries(metadata_retriever

        ffmpeg

        android
        jnigraphics
        log)

