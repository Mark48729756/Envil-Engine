# Shader toolchain for the Vulkan and Direct3D 12 backends.
#
# One source of truth: Vulkan-flavoured GLSL 4.50 in shaders/src/*.{vert,frag}
#   GLSL --glslc--> SPIR-V (.spv)                      -> Vulkan 1.4 / 1.2
#   SPIR-V --spirv-cross--> HLSL SM 6.x --dxc--> DXIL (.dxil)  -> Direct3D 12
#
# Tools come from the LunarG Vulkan SDK (glslc, spirv-cross, dxc); nothing is downloaded and no tests are run.
# Binding convention (see rhi/RHI.h):  set 0 = uniform buffers (b#),  set 1 = combined textures (t#/s#), set 2 = storage images, set 3 = storage buffers (compute),  push_constant = root constants.
#
#   anvil_compile_shaders(<target> <out_dir> [VULKAN] [D3D12] SOURCES a.vert a.frag ...)
# Outputs <out_dir>/<file>.spv and <out_dir>/<file>.dxil, and adds them as dependencies of <target>.
find_package(Vulkan QUIET COMPONENTS glslc)
find_program(ANVIL_GLSLC       glslc       HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin ${Vulkan_GLSLC_EXECUTABLE})
find_program(ANVIL_SPIRV_CROSS spirv-cross HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)
find_program(ANVIL_DXC         dxc         HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)

function(anvil_compile_shaders target out_dir)
  cmake_parse_arguments(A "VULKAN;D3D12" "" "SOURCES" ${ARGN})
  if(A_VULKAN AND NOT ANVIL_GLSLC)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK (https://vulkan.lunarg.com) and set VULKAN_SDK.")
  endif()
  if(A_D3D12 AND NOT (ANVIL_GLSLC AND ANVIL_SPIRV_CROSS AND ANVIL_DXC))
    message(FATAL_ERROR "D3D12 shaders need glslc + spirv-cross + dxc (all ship with the Vulkan SDK for Windows).")
  endif()
  file(MAKE_DIRECTORY ${out_dir})
  set(outputs "")
  foreach(src ${A_SOURCES})
    get_filename_component(name ${src} NAME)
    get_filename_component(ext  ${src} EXT)
    if(ext STREQUAL ".vert")
      set(stage vertex)   set(dxprofile vs_6_0)
    elseif(ext STREQUAL ".frag")
      set(stage fragment) set(dxprofile ps_6_0)
    elseif(ext STREQUAL ".comp")
      set(stage compute)  set(dxprofile cs_6_0)
    else()
      message(FATAL_ERROR "Unsupported shader stage for ${src}")
    endif()
    set(spv ${out_dir}/${name}.spv)
    add_custom_command(OUTPUT ${spv}
      COMMAND ${ANVIL_GLSLC} -fshader-stage=${stage} --target-env=vulkan1.2 -O -o ${spv} ${src}
      DEPENDS ${src} COMMENT "glslc ${name}" VERBATIM)
    list(APPEND outputs ${spv})
    if(A_D3D12)
      set(hlsl ${out_dir}/${name}.hlsl)
      set(dxil ${out_dir}/${name}.dxil)
      add_custom_command(OUTPUT ${hlsl}
        COMMAND ${ANVIL_SPIRV_CROSS} --hlsl --shader-model 60 --hlsl-enable-compat --output ${hlsl} ${spv}
        DEPENDS ${spv} COMMENT "spirv-cross ${name} -> HLSL" VERBATIM)
      add_custom_command(OUTPUT ${dxil}
        COMMAND ${ANVIL_DXC} -T ${dxprofile} -E main -O3 -Fo ${dxil} ${hlsl}
        DEPENDS ${hlsl} COMMENT "dxc ${name} -> DXIL" VERBATIM)
      list(APPEND outputs ${dxil})
    endif()
  endforeach()
  add_custom_target(${target} DEPENDS ${outputs})
endfunction()
