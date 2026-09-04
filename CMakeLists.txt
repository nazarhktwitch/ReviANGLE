cmake_minimum_required(VERSION 3.20)
project(ReviANGLE LANGUAGES CXX C
    DESCRIPTION  "OpenGL -> ANGLE -> D3D11 / Vulkan proxy for Geometry Dash"
    HOMEPAGE_URL "https://github.com/Reviusion/ReviANGLE"
    VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Backend selection options
option(REVIANGLE_BACKEND_D3D11 "Build with DirectX 11 backend (ANGLE)" ON)
option(REVIANGLE_BACKEND_VULKAN "Build with Vulkan backend" OFF)

if (NOT REVIANGLE_BACKEND_D3D11 AND NOT REVIANGLE_BACKEND_VULKAN)
    message(FATAL_ERROR "One backend must be enabled: REVIANGLE_BACKEND_D3D11 or REVIANGLE_BACKEND_VULKAN")
endif()

if (REVIANGLE_BACKEND_D3D11 AND REVIANGLE_BACKEND_VULKAN)
    message(FATAL_ERROR "Choose exactly one backend: D3D11 and Vulkan both provide WGL entry points")
endif()

message(STATUS "ReviANGLE backends: D3D11=${REVIANGLE_BACKEND_D3D11} Vulkan=${REVIANGLE_BACKEND_VULKAN}")

# Output directory structure: bin/ for executables, dll/ for libraries
set(REVIANGLE_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(REVIANGLE_DLL_DIR "${CMAKE_BINARY_DIR}/dll")

# Static-link MSVC runtime - eliminates MSVCP140.dll / VCRUNTIME140.dll
# external dependencies, makes DLL self-contained
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# GD 2.2+ is x64 - require x64 build
if (NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    message(WARNING "GD is 64-bit. Configure with: cmake -A x64")
endif()

# produce opengl32.dll (replaces the system one when placed next to GD.exe)
set(OUT_NAME "opengl32")

set(SRC
    # core
    src/dllmain.cpp
    src/config.cpp
    src/angle_loader.cpp
    src/gl_proxy.cpp
    src/gl_proxy_ext.cpp

    # common utilities
    src/common/iat_hook.cpp
    src/common/pe_utils.cpp
    src/common/dxt1_encoder.cpp

    # base boosts
    src/boost_gpu_forcer.cpp
    src/boost_allocator.cpp
    src/boost_timer.cpp
    src/boost_thread.cpp
    src/boost_math.cpp
    src/boost_power.cpp

    # advanced boosts
    src/boost_tex_compress.cpp
    src/boost_nvapi.cpp
    src/boost_shader_cache.cpp
    src/boost_laa.cpp
    src/boost_gl_dedup.cpp
    src/boost_prefetch.cpp
    src/boost_fmod.cpp
    src/boost_asset_loader.cpp
    src/boost_vsync.cpp
    src/boost_sleep.cpp
    src/boost_heap.cpp
    # render boosts
    src/boost_depth_off.cpp
    src/boost_mipmap_off.cpp
    src/boost_vbo_pool.cpp
    src/boost_vertex_compress.cpp
    src/boost_instancing.cpp
    src/boost_dyn_res.cpp

    # IO boosts
    src/boost_fast_io.cpp
    src/boost_ramdisk.cpp
    src/boost_loader_cache.cpp

    # CPU boosts
    src/boost_sse_memcpy.cpp    src/boost_string_intern.cpp
    src/boost_mimalloc_full.cpp
    src/boost_silent.cpp

    # system boosts
    src/boost_wddm_prio.cpp
    src/boost_gamemode.cpp
    src/boost_smart_pin.cpp
    src/boost_mitigation_off.cpp

    # anti-stutter
    src/boost_allow_tearing.cpp
    src/boost_waitable_swap.cpp
    src/boost_frame_pacing.cpp
    src/boost_mmcss.cpp
    src/boost_shader_warmup.cpp
    src/boost_low_latency.cpp

    # GD-specific
    src/boost_skip_intro.cpp
    src/boost_obj_pool.cpp    src/boost_plist_bin.cpp
    src/boost_skip_effects.cpp    src/boost_level_predecode.cpp
    src/boost_anti_stutter.cpp

    # advanced render    src/boost_fbo_cache.cpp
    src/boost_triple_buffer.cpp
    src/boost_no_aa.cpp
    src/boost_blend_opt.cpp

    # cocos2d-x
    src/boost_particle_throttle.cpp
    src/boost_texcache_preload.cpp    src/boost_label_cache.cpp
    src/boost_scheduler_skip.cpp
    src/boost_drawcall_sort.cpp
    src/boost_index_gen.cpp

    # system advanced
    src/boost_ftz_daz.cpp
    src/boost_spectre_off.cpp
    src/boost_io_priority.cpp
    src/boost_mem_priority.cpp
    src/boost_stack_trim.cpp

    # pipeline    src/boost_scissor_tight.cpp
    src/boost_vertex_dedup.cpp    src/boost_batch_coalesce.cpp

    # network
    src/boost_dns_prefetch.cpp
    src/boost_http_pool.cpp
    src/boost_online_block.cpp
    src/boost_server_cache.cpp
    src/boost_winsock_opt.cpp

    # audio
    src/boost_fmod_channels.cpp
    src/boost_fmod_softmix.cpp
    src/boost_audio_pin.cpp
    src/boost_sound_preload.cpp
    src/boost_audio_compress.cpp

    # extreme
    src/boost_etw_off.cpp
    src/boost_wer_off.cpp
    src/boost_smartscreen_off.cpp
    src/boost_numa.cpp
    src/boost_huge_pages.cpp
    src/boost_prefetcher_off.cpp

    # Windows-level extras
    src/boost_workingset_lock.cpp

    # opt-in optimizations
    src/boost_present_skip.cpp
    # v3.0.0 modules
    src/boost_d3d11_unlock.cpp
    src/boost_process_perf.cpp
    src/boost_pcore_pin.cpp
    src/boost_gpu_residency.cpp
    src/boost_stutter_monitor.cpp
    src/boost_no_fso.cpp
    src/boost_effect_lod.cpp
    src/boost_drawcall_budget.cpp
    src/boost_iocp_loader.cpp
    src/boost_thread_qos.cpp
)

# Conditional backend-specific sources
if (REVIANGLE_BACKEND_D3D11 OR REVIANGLE_BACKEND_VULKAN)
    list(APPEND SRC
        src/wgl_proxy.cpp
    )
endif()

add_library(${OUT_NAME} SHARED ${SRC} src/gl_exports.def)

target_include_directories(${OUT_NAME} PRIVATE
    src/
    angle/include/
)

# Link libraries based on selected backend
if (REVIANGLE_BACKEND_D3D11)
    target_link_libraries(${OUT_NAME} PRIVATE
        winmm       # timeBeginPeriod
        kernel32
        user32
        gdi32       # D3DKMT
        advapi32    # RegCreateKeyEx
        psapi       # GetModuleInformation
        dxgi        # IDXGIDevice1, IDXGIFactory5
        ws2_32      # Winsock (DNS prefetch, HTTP pool, etc.)
    )
    target_compile_definitions(${OUT_NAME} PRIVATE REVIANGLE_BACKEND_D3D11=1)
endif()

if (REVIANGLE_BACKEND_VULKAN)
    # Vulkan is selected through ANGLE at runtime. The DLL still exports OpenGL,
    # so keep the EGL/WGL bridge and let libEGL/libGLESv2 talk to Vulkan.
    target_compile_definitions(${OUT_NAME} PRIVATE REVIANGLE_BACKEND_VULKAN=1)
endif()

# Always link common libraries
target_link_libraries(${OUT_NAME} PRIVATE
    winmm
    kernel32
    user32
    ws2_32
)

target_compile_definitions(${OUT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

if (MSVC)
    target_compile_options(${OUT_NAME} PRIVATE /O2 /Oi)
    target_link_options(${OUT_NAME} PRIVATE /OPT:REF /OPT:ICF)
endif()

# output name exactly opengl32.dll (no lib prefix)
set_target_properties(${OUT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "opengl32"
    SUFFIX ".dll"
    LIBRARY_OUTPUT_DIRECTORY "${REVIANGLE_DLL_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${REVIANGLE_DLL_DIR}"
)

# ============================================================================
# Config editor (separate exe) - Win32 + DX11 + Dear ImGui
# Builds standalone gd-angle-editor.exe (~5 MB, no runtime deps).
# ============================================================================
set(EDITOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config_editor)
set(IMGUI_DIR  ${EDITOR_DIR}/third_party/imgui)

if (EXISTS ${IMGUI_DIR}/imgui.cpp)
    add_executable(gd_angle_editor WIN32
        ${EDITOR_DIR}/main.cpp
        ${EDITOR_DIR}/editor_app.cpp
        ${EDITOR_DIR}/ini_parser.cpp
        ${EDITOR_DIR}/schema.cpp

        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp

        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    )

    target_include_directories(gd_angle_editor PRIVATE
        ${EDITOR_DIR}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )

    target_link_libraries(gd_angle_editor PRIVATE
        d3d11
        dxgi
        d3dcompiler
        dwmapi
        imm32
    )

    target_compile_definitions(gd_angle_editor PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
    )

    if (MSVC)
        target_compile_options(gd_angle_editor PRIVATE /O2 /Oi /utf-8)
        target_link_options(gd_angle_editor PRIVATE /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS)
    endif()

    set_target_properties(gd_angle_editor PROPERTIES
        OUTPUT_NAME "gd-angle-editor"
        RUNTIME_OUTPUT_DIRECTORY "${REVIANGLE_DLL_DIR}"
    )
else()
    message(WARNING "gd_angle_editor skipped: ImGui sources not found at ${IMGUI_DIR}. Install ImGui or add the third_party/imgui folder to enable the editor build.")
endif()

# Build ReviANGLE Uninstaller
add_subdirectory(uninstaller)
