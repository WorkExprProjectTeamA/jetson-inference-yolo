
file(GLOB yolonetSources *.cpp)
file(GLOB yolonetIncludes *.h )

# libcurl 의존성 추가
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

cuda_add_executable(yolonet ${yolonetSources})

# MQTT 라이브러리 추가 (직접 링크)
find_library(PAHO_MQTT3C_LIB paho-mqtt3c)

# 라이브러리 링크에 CURL과 MQTT 추가
target_link_libraries(yolonet jetson-inference-yolo ${CURL_LIBRARIES} ${PAHO_MQTT3C_LIB})

# CURL 헤더 경로 추가
target_include_directories(yolonet PRIVATE ${CURL_INCLUDE_DIRS})

install(TARGETS yolonet DESTINATION bin)

# 기존 내용 주석 처리
#cuda_add_executable(yolonet ${yolonetSources})
#target_link_libraries(yolonet jetson-inference-yolo)
#install(TARGETS yolonet DESTINATION bin)

####
