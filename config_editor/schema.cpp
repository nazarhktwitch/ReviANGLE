#include "schema.hpp"

// Description style notes:
//  - desc_en / desc_ru: 1–3 short lines, plain text, no leading "EN:" prefix.
//  - status: short tag matching what's in angle_config.ini ([✅ ON], etc.).
//    UI strips the brackets and colors based on first ASCII letter.
//  - For Int options, min_int/max_int give the InputInt range. Use sensible
//    defaults (most are 0..65535).

static const OptionDef g_opts[] = {

    // ────────── [ANGLE] ─────────────────────────────────────────────────────
    {"ANGLE", "backend", OptType::Enum, "d3d11",
     "ANGLE renderer backend. d3d11 is the verified default; vulkan routes "
     "OpenGL through ANGLE's Vulkan renderer; d3d9 remains a legacy fallback "
     "for older GPUs / drivers.",
     "Renderer backend ANGLE. d3d11 - проверенный default; vulkan ведет OpenGL "
     "через Vulkan renderer ANGLE; d3d9 остается legacy fallback для старых "
     "GPU/драйверов.",
     "ON", 0, 65535, "d3d11,d3d9,vulkan"},
    {"ANGLE", "debug", OptType::Bool, "false",
     "Persistent debug log file. One fopen for the process lifetime - no "
     "per-call hitches. Default OFF for production; flip ON only when "
     "diagnosing init issues.",
     "Отладочный лог. Один fopen на весь процесс, без микрофризов. По "
     "умолчанию "
     "ВЫКЛ для production; включай только для диагностики init.",
     "OFF - production default"},
    {"ANGLE", "log_file", OptType::String, "angle_log.txt",
     "Path to the debug log file (relative to GD.exe).",
     "Путь к отладочному логу (относительно GD.exe).", "ON"},

    // ────────── [Boost] ─────────────────────────────────────────────────────
    {"Boost", "gpu_forcer", OptType::Bool, "true",
     "Exports NvOptimusEnablement=1 / AmdPowerXpressRequest=1 - forces the "
     "discrete GPU on Optimus / switchable-graphics laptops.",
     "Экспортирует NvOptimusEnablement / AmdPowerXpress - переключает "
     "Optimus / switchable-graphics ноут на дискретную видеокарту.",
     "ON"},
    {"Boost", "fast_allocator", OptType::Bool, "true",
     "Replaces the default heap allocator with a lock-free pool. ~3-5% "
     "win on alloc-heavy frames, no risk.",
     "Заменяет стандартный аллокатор на lock-free пул. ~3-5% буст на "
     "frame-ах с аллокациями, без риска.",
     "ON"},
    {"Boost", "timer_fix", OptType::Bool, "true",
     "timeBeginPeriod(1) - sub-millisecond Sleep granularity. Required for "
     "precise frame pacing.",
     "Включает 1 ms точность Sleep. Без этого пацер кадров теряет точность.",
     "ON"},
    {"Boost", "thread_boost", OptType::Bool, "true",
     "ABOVE_NORMAL process priority + main thread HIGHEST. Helps when other "
     "apps fight for CPU.",
     "ABOVE_NORMAL приоритет процесса + HIGHEST для главного потока. "
     "Помогает когда фон жрёт CPU.",
     "ON"},
    {"Boost", "cpu_affinity", OptType::Hex, "0",
     "CPU affinity mask in hex. 0 = let scheduler pick. With only 2 hardware "
     "threads on Ivy Bridge, manual pinning is wasteful.",
     "Маска CPU affinity (hex). 0 = автомат. На 2-поточном Ivy Bridge ручной "
     "pin бесполезен.",
     "ON"},
    {"Boost", "sse_math", OptType::Bool, "true",
     "SSE2 fast math intrinsics. Ivy Bridge supports AVX, totally safe.",
     "SSE2 fast-math инструкции. Ivy Bridge точно поддерживает.", "ON"},
    {"Boost", "power_boost", OptType::Bool, "true",
     "Disables Windows 10 PROCESS_POWER_THROTTLING (EcoQoS). Stops OS from "
     "downclocking GD when it thinks the app is idle.",
     "Отключает EcoQoS - операционка перестаёт даунклокать GD при "
     "\"простое\". Стабилизирует FPS.",
     "ON"},

    // ────────── [BoostAdvanced] ─────────────────────────────────────────────
    {"BoostAdvanced", "tex_compress", OptType::Bool, "false",
     "On-the-fly RGBA8 → DXT1 compression (4× less VRAM). Breaks rendering "
     "on ANGLE FL9 path - keep OFF on this hardware.",
     "Сжатие RGBA8 → DXT1 на лету (4× меньше VRAM). Ломает рендер на "
     "FL9-пути ANGLE - НЕ ВКЛЮЧАЙ.",
     "OFF - breaks textures on FL9 path"},
    {"BoostAdvanced", "nvapi_profile", OptType::Bool, "true",
     "NVAPI driver profile init - signals \"heavy GPU app, give me max perf\". "
     "Now correctly loads nvapi64.dll on x64 and uses void* return type "
     "(was sign-extending int → DEP crash).",
     "Инициализирует NVAPI чтобы драйвер включил max-perf профиль. "
     "Чинено: грузит nvapi64.dll на x64 и возвращает void* (раньше int → "
     "truncation → DEP).",
     "ON - verified initialized"},
    {"BoostAdvanced", "shader_cache", OptType::Bool, "true",
     "Saves compiled HLSL bytecode to disk. Big win on slow CPU - 2nd launch "
     "skips shader compilation stalls.",
     "Кеширует HLSL-байткод между запусками. Большой плюс на слабом CPU - "
     "со 2-го запуска нет stutter-ов от компиляции.",
     "ON"},
    {"BoostAdvanced", "shader_cache_dir", OptType::String, "shader_cache",
     "Folder for the shader cache (relative to GD.exe).",
     "Папка для кеша шейдеров (относительно GD.exe).", "ON"},
    {"BoostAdvanced", "large_address_aware", OptType::Bool, "true",
     "Patches PE header so GD can use 4 GB virtual memory instead of 2 GB. "
     "Useful for large levels with thousands of objects.",
     "Патчит PE-заголовок чтобы GD имел доступ к 4 GB virtual памяти "
     "вместо 2. Полезно для больших уровней.",
     "ON"},
    {"BoostAdvanced", "gl_state_dedup", OptType::Bool, "true",
     "Skips redundant glBindTexture / glUseProgram / glActiveTexture calls "
     "when state hasn't changed. Saves driver round-trips.",
     "Пропускает повторные glBindTexture/glUseProgram/glActiveTexture. "
     "Экономит обращения к драйверу.",
     "ON"},
    {"BoostAdvanced", "working_set_prefetch", OptType::Bool, "false",
     "Pre-faults GD code pages into RAM. OFF - caused init stalls on this "
     "hardware.",
     "Префолтит страницы кода GD. ВЫКЛ - на этом железе вызывает "
     "задержки при старте.",
     "OFF - causes init stalls"},
    {"BoostAdvanced", "fmod_tuning", OptType::Bool, "false",
     "FMOD audio engine tuning. OFF - IAT hook on FMOD crashes some builds.",
     "Тюнинг FMOD. ВЫКЛ - IAT hook на FMOD иногда крашит игру.",
     "OFF - IAT hook crash risk"},
    {"BoostAdvanced", "fmod_sample_rate", OptType::Int, "44100",
     "FMOD sample rate when fmod_tuning=true. Standard values: 44100, 48000.",
     "FMOD sample rate когда fmod_tuning=true. Стандарт: 44100 или 48000.",
     "OFF - fmod_tuning is off", 8000, 96000},
    {"BoostAdvanced", "async_asset_loader", OptType::Bool, "false",
     "Parallel asset loader (separate threadpool). OFF - only 2 hardware "
     "threads available, workers would starve the main loop.",
     "Параллельная загрузка ассетов. ВЫКЛ - у тебя 2 hw-потока, "
     "воркеры голодят главный цикл.",
     "OFF - only 2 CPU threads"},
    {"BoostAdvanced", "async_loader_threads", OptType::Int, "2",
     "Number of worker threads when async_asset_loader=true.",
     "Количество worker-потоков для async_asset_loader.",
     "OFF - async loader off", 1, 16},
    {"BoostAdvanced", "force_no_vsync", OptType::Bool, "true",
     "eglSwapInterval(0) - disables VSync at EGL level. Combined with "
     "unlock_fps_cap lets GPU run as fast as possible.",
     "eglSwapInterval(0) - отключает VSync на уровне EGL. Вместе с "
     "unlock_fps_cap позволяет GPU выдать максимум.",
     "ON"},
    {"BoostAdvanced", "precise_sleep", OptType::Bool, "true",
     "Replaces Sleep() with high-resolution spin-wait for ≤2 ms. IAT hook "
     "target wasn't found on your build (logged as failed) - harmless.",
     "Заменяет Sleep() на точный spin-wait. IAT hook не нашёл цель на "
     "этой сборке (видно в логе) - но включить безопасно.",
     "ON - IAT hook target not found, harmless"},
    {"BoostAdvanced", "heap_compact_interval", OptType::Int, "0",
     "Periodic HeapCompact() interval in seconds. 0 = disabled. Heap compact "
     "during gameplay causes 50-200 ms stutter - keep 0.",
     "Период HeapCompact() в секундах. 0 = выкл. HeapCompact во время "
     "геймплея даёт 50-200 ms заикания - оставь 0.",
     "0 = disabled", 0, 600},
    {"BoostAdvanced", "d3d11_multithread", OptType::Bool, "false",
     "ID3D11Multithread::SetMultithreadProtected. OFF - gated by backend; on "
     "D3D11 ANGLE manages threading itself, this conflicts.",
     "D3D11 multithread protection. ВЫКЛ - конфликтует с собственным "
     "threading-ом ANGLE на D3D11.",
     "OFF - ANGLE conflict"},

    // ────────── [BoostRender] ───────────────────────────────────────────────
    {"BoostRender", "depth_off", OptType::Bool, "false",
     "Disables depth test/clear. OFF - GD uses depth buffer for trigger "
     "ordering on certain levels.",
     "Выключает depth test/clear. ВЫКЛ - GD использует depth для "
     "порядка триггеров.",
     "OFF - breaks triggers"},
    {"BoostRender", "mipmap_off", OptType::Bool, "true",
     "Skips mipmap generation. Big VRAM/bandwidth save on weak/integrated "
     "GPUs. GD doesn't sample at distance, so no quality loss.",
     "Пропускает генерацию mipmap. Большая экономия VRAM/полосы на "
     "слабых/интегрированных GPU. Потери качества нет.",
     "ON - big win on weak GPUs"},
    {"BoostRender", "noop_finish", OptType::Bool, "true",
     "glFinish() → no-op. cocos2d sometimes calls Finish after frame which "
     "stalls the pipeline. Safe to skip.",
     "glFinish() становится no-op. cocos2d иногда зовёт Finish после "
     "кадра, это блокирует пайплайн. Безопасно.",
     "ON"},
    {"BoostRender", "noop_geterror", OptType::Bool, "true",
     "glGetError() → GL_NO_ERROR. Saves driver round-trip. Safe with "
     "gl_no_error context.",
     "glGetError() всегда GL_NO_ERROR. Экономит обращение к драйверу. "
     "Безопасно в паре с gl_no_error.",
     "ON"},
    {"BoostRender", "vbo_pool", OptType::Bool, "false",
     "Persistent VBO pool. OFF - cocos2d's batcher manages VBOs itself, "
     "overlap causes jitter.",
     "Пул VBO. ВЫКЛ - cocos2d сам управляет VBO, наш пул создаёт jitter.",
     "OFF - conflicts with cocos2d batcher"},
    {"BoostRender", "vbo_pool_size_mb", OptType::Int, "16",
     "VBO pool size in MB when vbo_pool=true.",
     "Размер VBO-пула в MB когда vbo_pool=true.", "OFF - vbo_pool off", 4, 256},
    {"BoostRender", "vertex_compress", OptType::Bool, "false",
     "Vertex format compression FP32 → FP16. OFF - risky, can break UV "
     "interpolation on text/UI sprites.",
     "Сжатие vertex-формата FP32 → FP16. ВЫКЛ - рискованно, может "
     "ломать UV на тексте/UI.",
     "OFF - risky"},
    {"BoostRender", "instancing", OptType::Bool, "false",
     "glDrawElementsInstanced batching. OFF - would require rewriting "
     "cocos2d draw calls.",
     "Инстансинг через glDrawElementsInstanced. ВЫКЛ - потребовал бы "
     "переписать cocos2d-draw.",
     "OFF - vertex layout mismatch"},
    {"BoostRender", "dyn_resolution", OptType::Bool, "false",
     "Dynamic resolution scaling on FPS dips. OFF - causes visible jitter, "
     "predictable FPS via frame pacing is better.",
     "Динамическое масштабирование разрешения. ВЫКЛ - даёт jitter, лучше "
     "предсказуемый FPS через пацер.",
     "OFF - visual jitter"},
    {"BoostRender", "dyn_res_target_fps", OptType::Int, "60",
     "Target FPS for dynamic resolution algorithm.",
     "Целевой FPS для алгоритма dynamic resolution.",
     "OFF - dyn_resolution off", 30, 240},

    // ────────── [BoostIO] ───────────────────────────────────────────────────
    {"BoostIO", "fast_io", OptType::Bool, "true",
     "CreateFile with FILE_FLAG_SEQUENTIAL_SCAN. IAT hook target wasn't "
     "found on this build - harmless when missing.",
     "CreateFile с FILE_FLAG_SEQUENTIAL_SCAN. IAT hook не нашёл цель на "
     "этой сборке - безопасно.",
     "ON - IAT hook target not found, harmless"},
    {"BoostIO", "ramdisk_cache", OptType::Bool, "false",
     "Copy Resources/ to RAM-backed temp. OFF - needs ~500 MB free RAM; "
     "you have 8 GB total which is tight.",
     "Копирует Resources/ в RAM. ВЫКЛ - нужно ~500 MB свободной RAM, "
     "у тебя 8 GB всего, это тесно.",
     "OFF - RAM-tight on 8 GB"},
    {"BoostIO", "ramdisk_path", OptType::String, "",
     "Custom path for ramdisk cache. Empty = auto.",
     "Свой путь для ramdisk-кеша. Пустой = авто.", "OFF - ramdisk_cache off"},
    {"BoostIO", "loader_cache", OptType::Bool, "true",
     "Cache GetProcAddress / wglGetProcAddress lookups. Big win - cocos2d "
     "re-resolves the same procs every frame.",
     "Кеш GetProcAddress / wglGetProcAddress. Большой плюс - cocos2d "
     "ре-резолвит одни и те же процы каждый кадр.",
     "ON"},

    // ────────── [BoostCPU] ──────────────────────────────────────────────────
    {"BoostCPU", "sse_memcpy", OptType::Bool, "false",
     "SSE2-optimized memcpy via IAT hook. OFF - hook crashes on some "
     "Windows runtimes (CRT version mismatch).",
     "SSE2 memcpy через IAT hook. ВЫКЛ - hook крашится на разных "
     "версиях Windows runtime.",
     "OFF - IAT hook crash risk"},
    {"BoostCPU", "scene_bvh", OptType::Bool, "false",
     "Bounding volume hierarchy for scene culling. OFF - experimental.",
     "BVH-дерево для отсечения сцены. ВЫКЛ - экспериментально.",
     "OFF - experimental"},
    {"BoostCPU", "string_intern", OptType::Bool, "false",
     "std::string interning via IAT hook. OFF - hooking string ctor "
     "crashes due to ABI variations.",
     "String interning через IAT hook на ctor-е std::string. ВЫКЛ - "
     "крашится из-за ABI-вариаций.",
     "OFF - IAT hook crash risk"},
    {"BoostCPU", "mimalloc_full", OptType::Bool, "false",
     "Replace global new/delete with mimalloc. OFF - mimalloc DLL not "
     "bundled with the mod.",
     "Заменяет global new/delete на mimalloc. ВЫКЛ - DLL не идёт в "
     "комплекте с модом.",
     "OFF - DLL not bundled"},
    {"BoostCPU", "silent_debug", OptType::Bool, "true",
     "Silences printf, OutputDebugString and other debug streams from "
     "cocos2d/GD. Small but free perf win.",
     "Глушит printf, OutputDebugString и debug-потоки cocos2d/GD. "
     "Маленький бесплатный буст.",
     "ON"},

    // ────────── [BoostSystem] ───────────────────────────────────────────────
    {"BoostSystem", "wddm_priority", OptType::Bool, "true",
     "D3DKMTSetSchedulingPriorityClass(HIGH) - Windows gives our render "
     "commands priority over other apps.",
     "Приоритет HIGH для GPU-контекста через WDDM. Windows отдаёт наши "
     "draw-вызовы вперёд других программ.",
     "ON"},
    {"BoostSystem", "game_mode", OptType::Bool, "true",
     "Registers GD with Windows Game Mode - disables non-essential "
     "background services.",
     "Регистрирует GD в Windows Game Mode - Windows вырубает фоновые "
     "сервисы.",
     "ON"},
    {"BoostSystem", "smart_cpu_pin", OptType::Bool, "true",
     "Pin main thread to P-cores on hybrid CPUs (Intel 12gen+). Ivy Bridge "
     "isn't hybrid - module is a no-op, safe to leave on.",
     "Пиннит главный поток на P-ядра на hybrid CPU (12gen+). Ivy Bridge "
     "не hybrid - no-op, безопасно держать.",
     "ON - no-op on Ivy Bridge"},
    {"BoostSystem", "mitigation_off", OptType::Bool, "false",
     "Disables Windows process mitigations (CFG, CET, strict ASLR). OFF - "
     "~1-2% gain not worth the security trade-off.",
     "Отключает Windows mitigations. ВЫКЛ - ~1-2% не стоит снижения "
     "безопасности.",
     "OFF - security trade-off"},

    // ────────── [BoostLatency] ──────────────────────────────────────────────
    {"BoostLatency", "allow_tearing", OptType::Bool, "true",
     "DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING. Gated by isFlipModel(swapEffect) - "
     "safely no-ops when ANGLE picks the legacy BLIT swap chain on older "
     "GPUs / drivers.",
     "Флаг ALLOW_TEARING. Гейчён по isFlipModel - безопасно становится "
     "no-op когда ANGLE выбирает legacy BLIT swap chain на старых "
     "GPU/драйверах.",
     "ON - gated by flip-model"},
    {"BoostLatency", "waitable_swap", OptType::Bool, "true",
     "DXGI waitable swap chain - waits on a handle instead of blocking "
     "inside Present(). Gated by backend==d3d11.",
     "DXGI waitable swap chain - ждёт на handle вместо блокировки в "
     "Present(). Гейчён по backend==d3d11.",
     "ON - D3D11 only"},
    {"BoostLatency", "frame_pacing", OptType::Bool, "true",
     "QPC-based frame pacing with sleep+spin loop. ON by default - paired "
     "with target=180 it gives the best gameplay feel: lowest input lag, "
     "smooth motion, no tearing. Set false only for raw FPS benchmarking.",
     "Frame pacing через QPC с sleep+spin. ВКЛ по умолчанию - в паре с "
     "target=180 даёт лучшее ощущение: минимальный input lag, "
     "плавность, без tearing-а. ВЫКЛ только для бенчмарка max FPS.",
     "ON - best feel preset"},
    {"BoostLatency", "frame_pacing_target", OptType::Int, "180",
     "Target FPS for QPC pacing. 180 = 2× refresh on a 90 Hz monitor - "
     "the lowest-input-lag sweet spot. Set 0 for auto-detect (matches "
     "refresh exactly), or 60/90 for VSync-style smoothness.",
     "Целевой FPS пацера. 180 = 2× refresh на 90 Hz мониторе - лучший "
     "баланс input lag. 0 = автоопределение (ровно по refresh), "
     "60/90 = плавность VSync-стиля.",
     "180 - 2× refresh on 90 Hz", 0, 1000},
    {"BoostLatency", "mmcss_pro_audio", OptType::Bool, "true",
     "Registers main thread as MMCSS Pro Audio class - gets 1 ms scheduling "
     "granularity (default is 15.6 ms quantum).",
     "Регистрирует главный поток как MMCSS Pro Audio - получает 1 ms "
     "гранулярность планирования (вместо 15.6 ms).",
     "ON"},
    {"BoostLatency", "shader_warmup", OptType::Bool, "false",
     "Pre-compile common shaders at init. OFF - can crash on shader "
     "compile errors; shader_cache covers warmup anyway.",
     "Предкомпиляция шейдеров при старте. ВЫКЛ - может падать; кеш на "
     "диске покрывает warmup.",
     "OFF - crash-risky"},
    {"BoostLatency", "low_latency", OptType::Bool, "true",
     "IDXGIDevice1::SetMaximumFrameLatency(1) - reduces input lag by "
     "~2 frames. Now hits correct vtable slot 12 (was slot 11 = "
     "GetGPUThreadPriority(INT*) which crashed with 'write to 0x1').",
     "IDXGIDevice1::SetMaximumFrameLatency(1) - снижает input lag на "
     "~2 кадра. Чинено: slot 12 (раньше 11 = GetGPUThreadPriority, "
     "крашило).",
     "ON - verified MaxFrameLatency=1"},
    {"BoostLatency", "gl_no_error", OptType::Bool, "true",
     "EGL_CONTEXT_OPENGL_NO_ERROR_KHR - disables ANGLE per-call validation. "
     "~10-20% CPU saving on weak Ivy Bridge.",
     "Контекст без валидации параметров GL. ~10-20% экономии CPU на "
     "слабом Ivy Bridge.",
     "ON - big win on weak CPU"},
    {"BoostLatency", "unlock_fps_cap", OptType::Bool, "true",
     "Hooks CCApplication::setAnimationInterval to remove cocos2d FPS cap "
     "(sets 1/1000 = 1 ms). Re-applied every second.",
     "Хукает CCApplication::setAnimationInterval чтобы убрать cocos2d "
     "FPS-cap (1/1000 = 1 ms). Переприменяется каждую секунду.",
     "ON"},
    {"BoostLatency", "anti_stutter", OptType::Bool, "true",
     "Disables Windows process affinity auto-update + EcoQoS thread "
     "throttling. Eliminates 0.5-2 ms hitches from core migration.",
     "Отключает автообновление affinity + EcoQoS-троттлинг потока. "
     "Убирает hitch-и 0.5-2 ms от миграции между ядрами.",
     "ON"},

    // ────────── [BoostGD] ───────────────────────────────────────────────────
    {"BoostGD", "skip_intro", OptType::Bool, "false",
     "Skip RobTop intro splash. OFF - file-loading hooks risk save "
     "corruption; intro is short anyway.",
     "Пропуск splash-экрана. ВЫКЛ - file-hook рискован для сейвов, "
     "интро короткое.",
     "OFF - risk to saves"},
    {"BoostGD", "object_pool", OptType::Bool, "false",
     "Object pool for cocos2d nodes. OFF - vtable corruption risk on "
     "CCNode subclasses.",
     "Пул объектов для cocos2d-нод. ВЫКЛ - риск порчи vtable.",
     "OFF - vtable corruption risk"},
    {"BoostGD", "object_pool_size", OptType::Int, "4096",
     "Pool capacity when object_pool=true.",
     "Ёмкость пула когда object_pool=true.", "OFF - object_pool off", 256,
     65535},
    {"BoostGD", "trigger_cache", OptType::Bool, "false",
     "Cache trigger evaluation results. OFF - caches go stale when "
     "triggers move during playback.",
     "Кеш результатов триггеров. ВЫКЛ - устаревает когда триггеры "
     "двигаются.",
     "OFF - stale on moving triggers"},
    {"BoostGD", "plist_binary", OptType::Bool, "false",
     "Binary plist cache. OFF - risk of corrupting save plist files.",
     "Бинарный кеш plist. ВЫКЛ - риск порчи save-файлов.",
     "OFF - save corruption risk"},
    {"BoostGD", "plist_cache_dir", OptType::String, "plist_cache",
     "Folder for binary plist cache.", "Папка для бинарного кеша plist.",
     "OFF - plist_binary off"},
    {"BoostGD", "skip_shake_flash", OptType::Bool, "false",
     "Skip screen shake / flash effects. OFF - heuristic hooks are "
     "glitch-prone, can hide effects on legit levels.",
     "Пропуск shake/flash. ВЫКЛ - эвристика глючит, прячет эффекты "
     "на нормальных уровнях.",
     "OFF - visual glitches"},
    {"BoostGD", "level_predecode", OptType::Bool, "false",
     "Pre-decode level on load (separate threads). OFF - only 2 CPU "
     "threads, race with GD's own decoder.",
     "Предекодирование уровня. ВЫКЛ - у тебя 2 CPU-потока, конкурирует "
     "с декодером GD.",
     "OFF - only 2 CPU threads"},
    {"BoostGD", "predecode_threads", OptType::Int, "2",
     "Number of decoder threads when level_predecode=true.",
     "Количество decoder-потоков для level_predecode.",
     "OFF - level_predecode off", 1, 8},

    // ────────── [BoostRenderAdv] ────────────────────────────────────────────
    {"BoostRenderAdv", "atlas_merge", OptType::Bool, "false",
     "Merge texture atlases on the fly. OFF - UV remapping breaks textures "
     "in custom levels.",
     "Слияние атласов на лету. ВЫКЛ - переназначение UV ломает текстуры "
     "в кастомных уровнях.",
     "OFF - breaks UVs"},
    {"BoostRenderAdv", "atlas_size", OptType::Int, "2048",
     "Atlas page size when atlas_merge=true.",
     "Размер страницы атласа для atlas_merge.", "OFF - atlas_merge off", 512,
     8192},
    {"BoostRenderAdv", "frustum_cull", OptType::Bool, "false",
     "Frustum culling on cocos2d nodes. OFF - can hide objects on screen "
     "edges due to incorrect bounding boxes.",
     "Frustum-отсечение нод. ВЫКЛ - может прятать объекты у краёв "
     "экрана из-за неверных bbox-ов.",
     "OFF - clips visible objects"},
    {"BoostRenderAdv", "fbo_cache", OptType::Bool, "false",
     "FBO pool. OFF - complicates render-target switching, jitter source.",
     "Пул FBO. ВЫКЛ - усложняет переключение RT, источник jitter.",
     "OFF - jitter source"},
    {"BoostRenderAdv", "fbo_pool_size", OptType::Int, "8",
     "FBO pool capacity when fbo_cache=true.",
     "Ёмкость пула FBO когда fbo_cache=true.", "OFF - fbo_cache off", 2, 64},
    {"BoostRenderAdv", "triple_buffer", OptType::Bool, "false",
     "Triple buffering hint. OFF - ANGLE doesn't honor our buffer-count "
     "hint, queued frames just add input lag.",
     "Тройная буферизация. ВЫКЛ - ANGLE игнорит hint, кадры в очереди = "
     "больше input lag.",
     "OFF - ineffective + adds lag"},
    {"BoostRenderAdv", "disable_aa", OptType::Bool, "true",
     "Disable GL_MULTISAMPLE / MSAA. HUGE win on integrated/weak GPUs - "
     "MSAA 2× costs 30-50% performance with no visible benefit at GD's "
     "resolution.",
     "Отключение MSAA. ОЧЕНЬ большой прирост на слабых/интегрированных GPU "
     "- MSAA 2× жрёт 30-50% без заметной разницы в качестве.",
     "ON - big win on weak GPUs"},
    {"BoostRenderAdv", "blend_optimize", OptType::Bool, "false",
     "Optimize blend modes. OFF - breaks fade transitions and additive "
     "blend.",
     "Оптимизация blend-модов. ВЫКЛ - ломает fade и additive blend.",
     "OFF - breaks transparency"},

    // ────────── [BoostCocos] ────────────────────────────────────────────────
    {"BoostCocos", "particle_throttle", OptType::Bool, "false",
     "Limit max active particles. Visual change - your call. Set "
     "particle_max=150 if your level chokes.",
     "Лимит активных частиц. Визуальное изменение, решай сам. "
     "Поставь particle_max=150 если уровень захлёбывается.",
     "OFF by default - visual choice"},
    {"BoostCocos", "particle_max", OptType::Int, "300",
     "Max simultaneous particles when particle_throttle=true.",
     "Макс одновременных частиц при particle_throttle=true.",
     "OFF - particle_throttle off", 50, 5000},
    {"BoostCocos", "texcache_preload", OptType::Bool, "false",
     "Pre-warm texture cache at startup. OFF - slows first launch by "
     "~3 s, no measurable in-game gain.",
     "Прогрев texture cache при старте. ВЫКЛ - замедляет первый запуск, "
     "в игре прироста нет.",
     "OFF - startup tax"},
    {"BoostCocos", "batch_force", OptType::Bool, "false",
     "Force-batch all sprites. OFF - modifies render state, sprite "
     "glitches.",
     "Принудительный batching спрайтов. ВЫКЛ - даёт глитчи на спрайтах.",
     "OFF - sprite glitches"},
    {"BoostCocos", "label_cache", OptType::Bool, "false",
     "Cache CCLabel-rendered glyphs. OFF - text recalc is cheap on "
     "cocos2d, not worth the complexity.",
     "Кеш отрендеренных глифов CCLabel. ВЫКЛ - пересчёт и так дешёв.",
     "OFF - minimal gain"},
    {"BoostCocos", "scheduler_skip", OptType::Bool, "true",
     "Skip inactive timers in CCScheduler tick. Free perf, completely safe.",
     "Пропуск неактивных таймеров в CCScheduler. Бесплатный буст, "
     "полностью безопасно.",
     "ON"},
    {"BoostCocos", "drawcall_sort", OptType::Bool, "false",
     "Sort draw calls by texture. OFF - breaks z-ordering of layered "
     "sprites in editor.",
     "Сортировка draw-вызовов по текстуре. ВЫКЛ - ломает z-порядок "
     "спрайтов в редакторе.",
     "OFF - breaks z-order"},
    {"BoostCocos", "index_buffer_gen", OptType::Bool, "false",
     "Generate index buffers for batched sprites. OFF - vertex layout "
     "mismatch with cocos2d expectations.",
     "Генерация index-буферов. ВЫКЛ - vertex layout не совпадает с "
     "cocos2d.",
     "OFF - layout mismatch"},

    // ────────── [BoostSysAdv] ───────────────────────────────────────────────
    {"BoostSysAdv", "ftz_daz", OptType::Bool, "false",
     "FTZ/DAZ FPU flags (flush-to-zero / denormals-are-zero). OFF - "
     "changes float behavior, can affect physics timing.",
     "FTZ/DAZ FPU флаги. ВЫКЛ - меняет поведение float, может повлиять "
     "на физику.",
     "OFF - affects physics"},
    {"BoostSysAdv", "spectre_off", OptType::Bool, "false",
     "Disable Spectre/Meltdown CPU mitigations for the process. OFF - "
     "~1-2% gain not worth security trade-off.",
     "Отключение Spectre/Meltdown mitigation. ВЫКЛ - безопасность не "
     "стоит ~1-2% прироста.",
     "OFF - security trade-off"},
    {"BoostSysAdv", "io_priority", OptType::Bool, "true",
     "NtSetInformationProcess(IoPriority=HIGH) - better disk priority "
     "for GD.",
     "NtSetInformationProcess(IoPriority=HIGH) - приоритет диска для GD.",
     "ON"},
    {"BoostSysAdv", "mem_priority", OptType::Bool, "true",
     "ProcessMemoryPriority=5 (highest non-realtime). Reduces page-out "
     "pressure during gameplay.",
     "ProcessMemoryPriority=5 (макс кроме realtime). Снижает выгрузку "
     "страниц во время игры.",
     "ON"},
    {"BoostSysAdv", "stack_trim", OptType::Bool, "false",
     "Trim main thread stack to 256 KB. OFF - cocos2d's CCNode tree "
     "traversal can recurse deeply, stack overflow risk.",
     "Обрезка стека до 256 KB. ВЫКЛ - обход CCNode-дерева может уходить "
     "в глубокую рекурсию, риск переполнения.",
     "OFF - recursion crash risk"},

    // ────────── [BoostPipeline] (all OFF - hot-path jitter) ────────────────
    {"BoostPipeline", "pipe_drawsort", OptType::Bool, "false",
     "Pipeline-level drawcall sort. OFF - hot-path hook = jitter on "
     "2-core CPU.",
     "Pipeline-сортировка draw. ВЫКЛ - hook hot-path = jitter на "
     "2-ядре.",
     "OFF - jitter on 2-core CPU"},
    {"BoostPipeline", "scissor_tight", OptType::Bool, "false",
     "Tighter scissor rectangles. OFF - can clip sprites at edges.",
     "Тесные scissor-прямоугольники. ВЫКЛ - может обрезать спрайты "
     "у краёв.",
     "OFF - can clip sprites"},
    {"BoostPipeline", "vertex_dedup", OptType::Bool, "false",
     "Dedup identical vertices in batches. OFF - hot path, jitter source.",
     "Дедупликация одинаковых вершин в batch. ВЫКЛ - hot path, jitter.",
     "OFF - hot-path jitter"},
    {"BoostPipeline", "halfres_effects", OptType::Bool, "false",
     "Render post-effects at half resolution. OFF - blurry visuals.",
     "Пост-эффекты в половинном разрешении. ВЫКЛ - мыло на эффектах.",
     "OFF - blurry visuals"},
    {"BoostPipeline", "shader_simplify", OptType::Bool, "false",
     "Patch shaders to simpler variants. OFF - risky shader rewriting.",
     "Патч шейдеров на упрощённые варианты. ВЫКЛ - рискованно.",
     "OFF - risky shader rewriting"},
    {"BoostPipeline", "batch_coalesce", OptType::Bool, "false",
     "Coalesce small draw batches. OFF - hot-path hook, jitter on "
     "2-core CPU.",
     "Объединение мелких batch-ей. ВЫКЛ - hook hot-path, jitter на "
     "2-ядре.",
     "OFF - jitter on 2-core CPU"},

    // ────────── [BoostNetwork] ──────────────────────────────────────────────
    {"BoostNetwork", "dns_prefetch", OptType::Bool, "true",
     "Pre-resolve GD server DNS at startup - hides 50-200 ms first-load "
     "latency.",
     "Предрезолв DNS серверов GD при старте - скрывает 50-200 ms первого "
     "запроса.",
     "ON"},
    {"BoostNetwork", "http_pool", OptType::Bool, "true",
     "HTTP keep-alive connection pool - reuses sockets between requests.",
     "HTTP keep-alive pool - переиспользует сокеты между запросами.", "ON"},
    {"BoostNetwork", "online_block_gameplay", OptType::Bool, "false",
     "Block all network during gameplay. OFF - would break online features "
     "(daily, gauntlets).",
     "Блокировка сети во время геймплея. ВЫКЛ - поломает онлайн "
     "(daily, gauntlets).",
     "OFF - breaks online features"},
    {"BoostNetwork", "server_cache", OptType::Bool, "true",
     "Cache GET responses (server lists, level previews) for 60 s.",
     "Кеш GET-ответов (списки серверов, превью) на 60 секунд.", "ON"},
    {"BoostNetwork", "winsock_opt", OptType::Bool, "true",
     "TCP_NODELAY + 64 KB send/recv buffers. Faster on small-packet "
     "network traffic.",
     "TCP_NODELAY + 64 KB буферы. Быстрее на мелких пакетах.", "ON"},

    // ────────── [BoostAudio] ────────────────────────────────────────────────
    {"BoostAudio", "fmod_channel_limit", OptType::Bool, "false",
     "Limit FMOD active channels. OFF - can mute legitimate SFX.",
     "Лимит активных FMOD-каналов. ВЫКЛ - может глушить нужные SFX.",
     "OFF - risk muting SFX"},
    {"BoostAudio", "fmod_max_channels", OptType::Int, "16",
     "Max FMOD channels when fmod_channel_limit=true.",
     "Макс FMOD-каналов при fmod_channel_limit=true.",
     "OFF - fmod_channel_limit off", 4, 64},
    {"BoostAudio", "fmod_software_mix", OptType::Bool, "false",
     "Force FMOD software mixing. OFF - loses GPU audio offload.",
     "Софтверный микс FMOD. ВЫКЛ - теряем GPU audio offload.",
     "OFF - loses GPU audio offload"},
    {"BoostAudio", "audio_thread_pin", OptType::Bool, "false",
     "Pin FMOD thread to dedicated core. OFF - on 2-core CPU starves main.",
     "Pin FMOD-потока на ядро. ВЫКЛ - на 2-ядерном CPU голодит main.",
     "OFF - only 2 cores"},
    {"BoostAudio", "sound_preload", OptType::Bool, "false",
     "Preload all .mp3/.ogg files. OFF - eats RAM (8 GB total is tight).",
     "Предзагрузка всех .mp3/.ogg. ВЫКЛ - жрёт RAM (всего 8 GB).",
     "OFF - RAM-tight"},
    {"BoostAudio", "audio_ram_compress", OptType::Bool, "false",
     "RAM compression of audio buffers. OFF - adds CPU cost, marginal "
     "gain.",
     "RAM-компрессия аудио. ВЫКЛ - CPU-нагрузка, выгода минимальная.",
     "OFF - CPU cost > gain"},

    // ────────── [BoostExtreme] ──────────────────────────────────────────────
    {"BoostExtreme", "etw_disable", OptType::Bool, "true",
     "Disable ETW event tracing for the process. Tiny but free win.",
     "Отключение ETW трассировки. Мелкий, но бесплатный буст.", "ON"},
    {"BoostExtreme", "wer_disable", OptType::Bool, "true",
     "Disable Windows Error Reporting for GD - no Watson dialogs or "
     "telemetry uploads on crashes.",
     "Отключение WER для GD - нет Watson-окон и телеметрии при крашах.", "ON"},
    {"BoostExtreme", "smartscreen_off", OptType::Bool, "true",
     "Strip Zone.Identifier ADS from our DLLs - bypasses SmartScreen "
     "\"downloaded from internet\" prompt.",
     "Удаляет Zone.Identifier ADS - обходит SmartScreen \"скачано из "
     "интернета\".",
     "ON"},
    {"BoostExtreme", "numa_aware", OptType::Bool, "true",
     "NUMA-aware memory allocation. Single NUMA node on Ivy Bridge "
     "laptop - module is a no-op.",
     "NUMA-aware аллокация. На Ivy Bridge один NUMA-узел - no-op.",
     "ON - no-op on single-NUMA"},
    {"BoostExtreme", "huge_pages", OptType::Bool, "false",
     "Use 2 MB large pages. OFF - needs SeLockMemoryPrivilege (admin).",
     "2 MB large pages. ВЫКЛ - требует SeLockMemoryPrivilege (admin).",
     "OFF - admin required"},
    {"BoostExtreme", "prefetcher_off", OptType::Bool, "false",
     "Disable Windows Superfetch/Prefetcher for GD. OFF - needs admin.",
     "Отключение Superfetch/Prefetcher для GD. ВЫКЛ - нужен admin.",
     "OFF - admin required"},
    {"BoostExtreme", "workingset_lock", OptType::Bool, "true",
     "Hard-pin minimum working set (SetProcessWorkingSetSizeEx + "
     "QUOTA_LIMITS_HARDWS_MIN_ENABLE). Stops Windows from paging our hot "
     "pages out under RAM pressure (browser/IDE/Defender scan). Falls "
     "back to a soft hint if SE_INC_WORKING_SET_NAME privilege denied.",
     "Хард-фиксация минимального working set - Windows не сможет выгрузить "
     "наши горячие страницы при нехватке RAM (браузер/IDE/Defender). Если "
     "привилегия SE_INC_WORKING_SET_NAME недоступна - мягкий hint.",
     "ON - anti-stutter on RAM-tight systems"},
    {"BoostExtreme", "gpu_thread_prio", OptType::Bool, "true",
     "IDXGIDevice::SetGPUThreadPriority(+7). Bumps the user-mode driver's "
     "GPU command-list submission thread priority. Default is 0; range "
     "[-7..+7]. Helps win against DWM compositor / browser GPU process. "
     "Auto-clamps to +5 / +2 if elevation needed; harmless on any case.",
     "Приоритет GPU thread в user-mode драйвере = +7 (макс). Помогает "
     "выиграть конкуренцию с DWM-композитором / GPU-процессом браузера. "
     "При нехватке прав авто-clamp до +5 / +2.",
     "ON - boosts GPU command submission"},

    // ===== (opt-in pipeline) =====
    {"BoostExtreme", "present_skip_idle", OptType::Bool, "false",
     "Skip eglSwapBuffers when the frame had zero draw calls "
     "(idle menus, paused level, dialog open). Capped at 4 consecutive "
     "skips so input latency stays bounded. Saves GPU power on idle "
     "scenes - fan stops ramping up while menus are open.",
     "Пропуск eglSwapBuffers на кадрах без draw-call'ов (главное меню, "
     "пауза, диалог). Не больше 4 подряд - чтобы input lag оставался "
     "ограниченным. Экономит мощность GPU на идле - куллер не разгоняется "
     "пока открыто меню.",
     "OFF - opt-in (test on your hardware first)"},

    {"BoostExtreme", "halfres_render", OptType::Bool, "false",
     "Render the entire game at half resolution (W/2 × H/2) into an "
     "offscreen FBO, then upscale to the real backbuffer via "
     "glBlitFramebuffer with GL_LINEAR filter on Present. Trades visible "
     "quality (UI text becomes blurrier) for ~30-50% less fragment "
     "shading work. Big win on weak GPUs (GT 630M-class fillrate-bound).",
     "Рендер всей игры в половинном разрешении (W/2 × H/2) в offscreen "
     "FBO, апскейл до реального backbuffer через glBlitFramebuffer с "
     "линейной фильтрацией на Present. Жертвуешь качеством (UI текст "
     "блюрнее) ради ~30-50% меньше работы по фрагмент-шейдингу. Огромный "
     "выигрыш на слабых GPU (типа GT 630M, fillrate-bound).",
     "OFF - opt-in (visible quality drop)"},
};

const std::vector<OptionDef> &schemaAll() {
  static const std::vector<OptionDef> v(std::begin(g_opts), std::end(g_opts));
  return v;
}

const std::vector<std::string> &schemaSections() {
  static const std::vector<std::string> v = {
      "ANGLE",         "Boost",          "BoostAdvanced", "BoostRender",
      "BoostIO",       "BoostCPU",       "BoostSystem",   "BoostLatency",
      "BoostGD",       "BoostRenderAdv", "BoostCocos",    "BoostSysAdv",
      "BoostPipeline", "BoostNetwork",   "BoostAudio",    "BoostExtreme",
  };
  return v;
}
