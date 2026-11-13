add_library(anvil STATIC
  src/Controller.cpp src/Import.cpp src/ProjectHub.cpp src/UI.cpp src/Window.cpp src/Shader.cpp src/Mesh.cpp src/Renderer.cpp src/RhiRenderer.cpp src/Application.cpp src/Scene.cpp src/SceneSerializer.cpp src/AssetDatabase.cpp src/Texture2D.cpp src/GpuProfiler.cpp src/PhysicsWorld.cpp src/physics/PhysicsBackend.cpp src/rhi/OpenGLRHI.cpp src/rhi/ResourceManager.cpp src/rhi/RHI.cpp src/rhi/ShaderBlob.cpp)
target_include_directories(anvil PUBLIC include)
target_include_directories(anvil PRIVATE src)
target_link_libraries(anvil PUBLIC glfw glm::glm glad_gl45)
if(ANDROID)
  # ControllerSystem consumes AInputEvent objects forwarded by the NativeActivity/GameActivity input queue.
  target_link_libraries(anvil PUBLIC android)
endif()

if(ANVIL_WITH_STEAM_INPUT)
  find_path(ANVIL_STEAMWORKS_INCLUDE_DIR steam/steam_api.h
    HINTS ENV STEAMWORKS_SDK_ROOT
    PATH_SUFFIXES public)
  find_library(ANVIL_STEAMWORKS_LIBRARY NAMES steam_api64 steam_api
    HINTS ENV STEAMWORKS_SDK_ROOT
    PATH_SUFFIXES redistributable_bin/linux64 redistributable_bin/win64 lib/linux64 lib/win64)
  if(NOT ANVIL_STEAMWORKS_INCLUDE_DIR OR NOT ANVIL_STEAMWORKS_LIBRARY)
    message(FATAL_ERROR "ANVIL_WITH_STEAM_INPUT=ON requires Steamworks SDK headers (steam/steam_api.h) and steam_api library. Set STEAMWORKS_SDK_ROOT.")
  endif()
  target_include_directories(anvil PRIVATE "${ANVIL_STEAMWORKS_INCLUDE_DIR}")
  target_link_libraries(anvil PUBLIC "${ANVIL_STEAMWORKS_LIBRARY}")
  target_compile_definitions(anvil PUBLIC ANVIL_WITH_STEAM_INPUT=1)
endif()
if(ZLIB_FOUND)
  target_link_libraries(anvil PRIVATE ZLIB::ZLIB)
  target_compile_definitions(anvil PRIVATE ANVIL_HAS_ZLIB=1)
endif()
target_compile_definitions(anvil PUBLIC ANVIL_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders" GLM_ENABLE_EXPERIMENTAL)
if(MSVC)
  target_compile_options(anvil PRIVATE /W4)
else()
  target_compile_options(anvil PRIVATE -Wall -Wextra)
endif()


if(ANVIL_WITH_JOLT)
  include(${CMAKE_SOURCE_DIR}/cmake/AnvilJolt.cmake)
  target_sources(anvil PRIVATE src/JoltPhysicsWorld.cpp src/physics/JoltBackend.cpp)
  target_link_libraries(anvil PUBLIC Jolt)
  target_compile_definitions(anvil PUBLIC ANVIL_WITH_JOLT=1)
endif()

if(ANVIL_WITH_D3D12)
  target_sources(anvil PRIVATE src/rhi/D3D12RHI.cpp)
  target_link_libraries(anvil PUBLIC d3d12 dxgi dxguid)
  target_compile_definitions(anvil PUBLIC ANVIL_WITH_D3D12=1)
  include(${CMAKE_SOURCE_DIR}/cmake/AnvilShaders.cmake)
  anvil_compile_shaders(anvil_d3d12_compute_validation_shader ${CMAKE_BINARY_DIR}/shaders/rhi D3D12
    SOURCES ${CMAKE_SOURCE_DIR}/shaders/rhi/compute_validation.comp)
  add_executable(anvil_d3d12_compute_validation ${CMAKE_SOURCE_DIR}/tests/d3d12/D3D12ComputeValidation.cpp)
  add_dependencies(anvil_d3d12_compute_validation anvil_d3d12_compute_validation_shader)
  target_link_libraries(anvil_d3d12_compute_validation PRIVATE d3d12 dxgi dxguid d3dcompiler)
  target_compile_features(anvil_d3d12_compute_validation PRIVATE cxx_std_20)
  if(BUILD_TESTING)
    add_test(NAME anvil_d3d12_compute_validation
      COMMAND anvil_d3d12_compute_validation ${CMAKE_BINARY_DIR}/shaders/rhi/compute_validation.comp.dxil)
    set_tests_properties(anvil_d3d12_compute_validation PROPERTIES SKIP_RETURN_CODE 77)
  endif()
endif()

if(ANVIL_WITH_VULKAN)
  find_package(Vulkan REQUIRED)
  FetchContent_Declare(vma GIT_REPOSITORY https://github.com/GPUOpen-LIBRARIES/VulkanMemoryAllocator.git GIT_TAG v3.3.0 GIT_SHALLOW TRUE SOURCE_SUBDIR _none)
  FetchContent_MakeAvailable(vma)
  target_sources(anvil PRIVATE src/rhi/VulkanRHI.cpp)
  target_include_directories(anvil SYSTEM PRIVATE ${vma_SOURCE_DIR}/include)
  target_link_libraries(anvil PUBLIC Vulkan::Vulkan)
  target_compile_definitions(anvil PUBLIC ANVIL_WITH_VULKAN=1)
  # M35: Vulkan-flavoured GLSL for RhiRenderer -> SPIR-V (glslc), built automatically with the engine.
  include(${CMAKE_SOURCE_DIR}/cmake/AnvilShaders.cmake)
  set(ANVIL_RHI_SHADER_SRC ${CMAKE_SOURCE_DIR}/shaders/rhi)
  set(ANVIL_RHI_SHADER_MODES VULKAN)
  if(ANVIL_COMPILE_D3D12_SHADERS)
    list(APPEND ANVIL_RHI_SHADER_MODES D3D12)
  endif()
  anvil_compile_shaders(anvil_rhi_shaders ${CMAKE_BINARY_DIR}/shaders/rhi ${ANVIL_RHI_SHADER_MODES} SOURCES
    ${ANVIL_RHI_SHADER_SRC}/fullscreen.vert ${ANVIL_RHI_SHADER_SRC}/gbuffer.vert ${ANVIL_RHI_SHADER_SRC}/gbuffer.frag
    ${ANVIL_RHI_SHADER_SRC}/shadow.vert ${ANVIL_RHI_SHADER_SRC}/shadow.frag ${ANVIL_RHI_SHADER_SRC}/lighting.frag
    ${ANVIL_RHI_SHADER_SRC}/bloom_down.frag ${ANVIL_RHI_SHADER_SRC}/bloom_up.frag ${ANVIL_RHI_SHADER_SRC}/post.frag
    ${ANVIL_RHI_SHADER_SRC}/blit.frag)
  add_dependencies(anvil anvil_rhi_shaders)
  target_compile_definitions(anvil PRIVATE ANVIL_RHI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders/rhi")
endif()


# DXIL generation is deliberately separate from ANVIL_WITH_D3D12: compiling
# shaders does not mean a native D3D12 device/swapchain backend is available.
if(ANVIL_COMPILE_D3D12_SHADERS AND NOT ANVIL_WITH_VULKAN)
  include(${CMAKE_SOURCE_DIR}/cmake/AnvilShaders.cmake)
  set(ANVIL_RHI_SHADER_SRC ${CMAKE_SOURCE_DIR}/shaders/rhi)
  anvil_compile_shaders(anvil_d3d12_shaders ${CMAKE_BINARY_DIR}/shaders/rhi D3D12 SOURCES
    ${ANVIL_RHI_SHADER_SRC}/fullscreen.vert ${ANVIL_RHI_SHADER_SRC}/gbuffer.vert ${ANVIL_RHI_SHADER_SRC}/gbuffer.frag
    ${ANVIL_RHI_SHADER_SRC}/shadow.vert ${ANVIL_RHI_SHADER_SRC}/shadow.frag ${ANVIL_RHI_SHADER_SRC}/lighting.frag
    ${ANVIL_RHI_SHADER_SRC}/bloom_down.frag ${ANVIL_RHI_SHADER_SRC}/bloom_up.frag ${ANVIL_RHI_SHADER_SRC}/post.frag
    ${ANVIL_RHI_SHADER_SRC}/blit.frag)
  add_dependencies(anvil anvil_d3d12_shaders)
  target_compile_definitions(anvil PRIVATE ANVIL_RHI_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders/rhi")
endif()
