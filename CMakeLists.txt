cmake_minimum_required(VERSION 3.24)
project(Opra VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /MP)
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Shaders: HLSL -> DXIL, compiled by the Windows SDK's dxc at build time.
# ---------------------------------------------------------------------------
find_program(DXC_EXECUTABLE dxc)

if(NOT DXC_EXECUTABLE)
    file(GLOB _dxc_candidates "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/10.*/x64/dxc.exe")
    if(_dxc_candidates)
        list(SORT _dxc_candidates)
        list(REVERSE _dxc_candidates)
        list(GET _dxc_candidates 0 DXC_EXECUTABLE)
    endif()
endif()

if(NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "dxc.exe not found. Install the Windows SDK or add dxc to PATH.")
endif()
message(STATUS "dxc: ${DXC_EXECUTABLE}")

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders")

# name:file:stage:entry
set(OPRA_SHADERS
    "mesh_vs:mesh.hlsl:vs_6_0:VSMain"
    "mesh_vs_flame:mesh.hlsl:vs_6_0:VSFlame"
    "mesh_ps:mesh.hlsl:ps_6_0:PSMain"
    "mesh_ps_ambient:mesh.hlsl:ps_6_0:PSAmbient"
    "mesh_ps_flame:mesh.hlsl:ps_6_0:PSFlame"
    "ui_vs:ui.hlsl:vs_6_0:VSMain"
    "ui_ps:ui.hlsl:ps_6_0:PSMain"
    "shadow_vs:shadow.hlsl:vs_6_0:VSMain"
    "shadow_ps:shadow.hlsl:ps_6_0:PSMain"
    "planet_vs:planet.hlsl:vs_6_0:VSMain"
    "planet_ps:planet.hlsl:ps_6_0:PSMain"
    "planet_air_ps:planet.hlsl:ps_6_0:PSAir"
    "star_vs:star.hlsl:vs_6_0:VSMain"
    "star_ps:star.hlsl:ps_6_0:PSMain"
    "bloom_vs:bloom.hlsl:vs_6_0:VSFullscreen"
    "bloom_threshold_ps:bloom.hlsl:ps_6_0:PSThreshold"
    "bloom_downsample_ps:bloom.hlsl:ps_6_0:PSDownsample"
    "bloom_upsample_ps:bloom.hlsl:ps_6_0:PSUpsample"
    "bloom_combine_ps:bloom.hlsl:ps_6_0:PSCombine"
    "tonemap_ps:tonemap.hlsl:ps_6_0:PSMain"
)

set(SHADER_OUTPUTS)
foreach(entry IN LISTS OPRA_SHADERS)
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 file)
    list(GET parts 2 stage)
    list(GET parts 3 entrypoint)
    set(out "${SHADER_OUT}/${name}.dxil")
    add_custom_command(
        OUTPUT "${out}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUT}"
        COMMAND "${DXC_EXECUTABLE}" -T ${stage} -E ${entrypoint} -O3
                "${SHADER_DIR}/${file}" -Fo "${out}"
        DEPENDS "${SHADER_DIR}/${file}"
        COMMENT "dxc ${file} -> ${name}.dxil"
        VERBATIM
    )
    list(APPEND SHADER_OUTPUTS "${out}")
endforeach()

add_custom_target(opra_shaders DEPENDS ${SHADER_OUTPUTS})

# ---------------------------------------------------------------------------
# Game
# ---------------------------------------------------------------------------
add_executable(Opra
    src/main.cpp
    src/selftest.cpp
    src/selftest_support.cpp
    src/orbit/encounter.cpp
    src/orbit/encounter_tests.cpp
    src/orbit/lagrange.cpp
    src/orbit/lagrange_tests.cpp
    src/orbit/conic.cpp
    src/orbit/kepler.cpp
    src/orbit/soi.cpp
    src/orbit/tests.cpp
    src/orbit/transfer.cpp
    src/orbit/transfer_tests.cpp
    src/sim/atmosphere.cpp
    src/sim/belt.cpp
    src/sim/descent.cpp
    src/sim/dock.cpp
    src/sim/dock_tests.cpp
    src/sim/system.cpp
    src/sim/system_tests.cpp
    src/sim/collision.cpp
    src/sim/combat.cpp
    src/sim/combat_tests.cpp
    src/sim/component.cpp
    src/sim/component_tests.cpp
    src/sim/physics.cpp
    src/sim/program.cpp
    src/sim/shapes.cpp
    src/sim/survey.cpp
    src/sim/terrain.cpp
    src/sim/shapes_tests.cpp
    src/sim/world.cpp
    src/sim/worlds_tests.cpp
    src/sim/world_tests.cpp
    src/core/crash.cpp
    src/core/file.cpp
    src/core/rng.cpp
    src/game/bindings.cpp
    src/game/camera_follow.cpp
    src/game/tape.cpp
    src/game/effects.cpp
    src/game/effects_tests.cpp
    src/game/frame.cpp
    src/game/loop.cpp
    src/game/scene.cpp
    src/game/settings.cpp
    src/game/viewer.cpp
    src/game/warp.cpp
    src/game/warp_tests.cpp
    src/ui/flow.cpp
    src/ui/flow_tests.cpp
    src/ui/menus.cpp
    src/ui/table.cpp
    src/ui/title.cpp
    src/ui/ui.cpp
    src/game/app.cpp
    src/game/capture.cpp
    src/game/input.cpp
    src/gpu/gpu.cpp
    src/render/backdrop.cpp
    src/render/camera.cpp
    src/render/material.cpp
    src/render/gltf.cpp
    src/render/orrery.cpp
    src/render/orrery_tests.cpp
    src/render/mesh.cpp
    src/render/texture.cpp
    src/render/model.cpp
    src/render/renderer.cpp
    src/render/scene.cpp
    src/render/text.cpp
    src/render/text_tests.cpp
    src/ui/draw.cpp
    src/ui/screens.cpp
    src/hud/blocks.cpp
    src/hud/director.cpp
    src/hud/minimap.cpp
    src/hud/hud.cpp
)

target_include_directories(Opra PRIVATE src)
target_link_libraries(Opra PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    glm::glm
)
add_dependencies(Opra opra_shaders)

# Shaders live next to the executable at runtime.
add_custom_command(TARGET Opra POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SHADER_OUT}" "$<TARGET_FILE_DIR:Opra>/shaders"
)
