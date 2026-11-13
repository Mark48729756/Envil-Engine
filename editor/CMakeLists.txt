add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC glfw)

add_executable(anvil_editor main.cpp Icons.cpp)
target_include_directories(anvil_editor PRIVATE ${nanosvg_SOURCE_DIR}/src)
target_link_libraries(anvil_editor PRIVATE anvil imgui_lib)
target_compile_definitions(anvil_editor PRIVATE ANVIL_ROOT_DIR="${CMAKE_SOURCE_DIR}")
