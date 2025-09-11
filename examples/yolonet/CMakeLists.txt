
file(GLOB yolonetSources *.cpp)
file(GLOB yolonetIncludes *.h )

cuda_add_executable(yolonet ${yolonetSources})
target_link_libraries(yolonet jetson-inference-yolo)
install(TARGETS yolonet DESTINATION bin)