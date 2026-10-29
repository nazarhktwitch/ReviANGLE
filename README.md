<div align="center">

# ReviANGLE

**A drop-in `opengl32.dll` proxy that routes Geometry Dash's OpenGL through Google ANGLE → DirectX 11 or Vulkan.**

*Unlock FPS, reduce input lag, eliminate microstutters - all on hardware Geometry Dash never officially targeted. Now with full Vulkan backend support.*

[![Release Actions](https://github.com/nazarhktwitch/ReviANGLE/actions/workflows/release.yml/badge.svg)](https://github.com/nazarhktwitch/ReviANGLE/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/nazarhktwitch/ReviANGLE)](https://github.com/nazarhktwitch/ReviANGLE/releases)

[English](#english) · [Русский](#russian)

</div>

---

## English

### About This Fork

This is an **active fork** of the original [ReviANGLE](https://github.com/Reviusion/ReviANGLE) project with **simultaneous support for both DirectX 11 and Vulkan backends**. Maintained at [@nazarhktwitch/ReviANGLE](https://github.com/nazarhktwitch/ReviANGLE). The original repository remains monitored for upstream updates which will be integrated as they become available.

**Key differences from the original:**
- **In-Game Overlay Configurator**: Press **`Alt + Home`** in-game for real-time live parameter tuning without restarting.
- **Vulkan backend** built from ANGLE source specifically for this project (custom DLL compilation)
- **Mod Compatibility**: Restored legacy OpenGL 1.1 state exports & attribute stack for flawless overlay rendering
- **Dual-backend support**: Choose DirectX 11 or Vulkan per build
- **Automated CI/CD Releases**: Clean source compilation via GitHub Actions & local `build_release.ps1` packager
- **Uninstaller**: Auto-detecting uninstaller

---

### What is it?

ReviANGLE is a performance mod for **Geometry Dash 2.2** that replaces the game's `opengl32.dll` with a custom proxy. The proxy:

1. **Translates OpenGL → DirectX 11 or Vulkan** via [Google ANGLE](https://chromium.googlesource.com/angle/angle) - pick the backend that suits your GPU:
   - **DirectX 11** (recommended by default): High compatibility, hardware DXGI Allow Tearing (Flip Model) support, and maximum peak throughput in uncapped benchmarks on NVIDIA GPUs.
   - **Vulkan**: Modern low-level backend for Linux/Proton and modern GPUs. Performance varies depending on your system configuration: Vulkan bypasses DXGI overlay hook conflicts (often delivering smoother frame pacing without stutters), though it may show lower peak numbers in empty uncapped scenes on Windows compared to DirectX 11.
2. Adds **84 low-level performance modules** that hook into ANGLE's hot path:
   - GL state deduplication (skip 30-50 % of redundant cocos2d-x calls)
   - High-resolution frame pacing (no CPU spin)
   - DXGI low-latency present (1-frame queue)
   - Optional half-resolution rendering with linear upscale (~30-50 % GPU win on weak GPUs)
   - Optional idle-frame Present elision (saves GPU power on menus)
   - NVAPI driver profile (PSTATE=P0, max-perf, no driver vsync)
   - Working-set lock so Windows doesn't page our hot data out
   - GPU thread priority bump
   - 40+ other tweaks - see [`docs/CONFIG_REFERENCE.md`](docs/CONFIG_REFERENCE.md)
3. Ships with **ReviANGLE Studio** - a standalone GUI configurator with bilingual (EN/RU) descriptions for every option.

### Why?

Vanilla GD was written for OpenGL on hardware that's now **15+ years old**. On weak laptops (Intel HD / GT 630M / etc.) it stutters, caps at 60 FPS, and wastes huge amounts of CPU on redundant driver calls. ReviANGLE rewrites that pipeline.

### Quick install

1. Go to [**Releases**](https://github.com/nazarhktwitch/ReviANGLE/releases) and download:
   - **DirectX 11 build** (default): `ReviANGLE-vX.Y.Z-DX11.zip`
   - **Vulkan build** (modern GPUs): `ReviANGLE-vX.Y.Z-Vulkan.zip`
2. **Backup** your `Geometry Dash` folder (or at least the original `opengl32.dll` if it exists).
3. Extract the ZIP into your Geometry Dash install directory (where `GeometryDash.exe` lives).

**DirectX 11 build** includes:
```text
Geometry Dash/
├── GeometryDash.exe
├── opengl32.dll              ← from ReviANGLE
├── libEGL.dll                ← from ANGLE
├── libGLESv2.dll             ← from ANGLE
├── d3dcompiler_47.dll        ← DirectX 11 compiler
├── angle_config.ini          ← config (editable)
├── gd-angle-editor.exe       ← GUI configurator
└── ReviANGLE-Uninstall.exe   ← Uninstaller
```

**Vulkan build** includes:
```text
Geometry Dash/
├── GeometryDash.exe
├── opengl32.dll              ← from ReviANGLE
├── libEGL.dll                ← from ANGLE
├── libGLESv2.dll             ← from ANGLE
├── vulkan-1.dll              ← Vulkan runtime
├── angle_config.ini          ← config (editable)
├── gd-angle-editor.exe       ← GUI configurator
└── ReviANGLE-Uninstall.exe   ← Uninstaller
```

4. Launch GD. If everything works, `angle_log.txt` will appear next to the `.exe`.

**Cannot decide which one?** Start with **DirectX 11** - it's the most stable. If you have an RTX or modern Radeon GPU and want max performance, try Vulkan.

#### Linux & Steam Deck (Proton / Wine)
ReviANGLE supports Linux / Steam Deck via Proton:
1. Extract the ZIP files into your GD folder next to `GeometryDash.exe`.
2. In Steam, right-click **Geometry Dash ➔ Properties ➔ Launch Options** and enter:
   ```bash
   WINEDLLOVERRIDES="opengl32=n,b" %command%
   ```

### Configure

Run **`gd-angle-editor.exe`** for a GUI:

- Bilingual descriptions (English + Russian) for every option
- Backend selector (DirectX 11 / Vulkan / DirectX 9)
- Comments and section structure preserved on save (round-trip safe)
- One-click "Reset to defaults" applies the **best-feel preset** for the tested hardware

Or edit `angle_config.ini` directly - it's plain text with full bilingual comments.

### Building from source

See [`docs/BUILDING.md`](docs/BUILDING.md). TL;DR:

```powershell
# Prerequisites: Visual Studio 2022 (C++ workload), CMake 3.20+
git clone https://github.com/nazarhktwitch/ReviANGLE.git
cd ReviANGLE

# Automated release packaging (both DX11 and Vulkan builds):
# Change "v1.1.0" to the desired version
.\build_release.ps1 -Version "v1.1.0"

# Or manual CMake build (e.g. DX11):
cmake -B build_dx11 -A x64 -DREVIANGLE_BACKEND_D3D11=ON -DREVIANGLE_BACKEND_VULKAN=OFF
cmake --build build_dx11 --config Release
```

Output binaries go to `build_dx11\dll\Release\` (`opengl32.dll`, `gd-angle-editor.exe`) and `build_dx11\bin\Release\` (`ReviANGLE-Uninstall.exe`).

Prebuilt ANGLE dependencies are staged under `deps/dx11` and `deps/vulkan`.


### Compatibility

| Compatible | Status |
|------------|--------|
| Geometry Dash 2.2 (Steam, standalone) | ✅ tested |
| MegaHack | ✅ tested (compatible) |
| Eclipse Menu | ✅ tested (compatible) |
| Linux | ✅ Supported via Proton / Wine |
| Mac OS | ❌ Too much work... |

### Credits & acknowledgements

- **ANGLE** team at Google for the GLES → D3D translation library.
- **cocos2d-x** authors - GD's underlying engine.
- **RobTop Games** - Geometry Dash itself (this mod is unaffiliated).
- **Dear ImGui** - used for the configurator GUI.
- **Original project**: Reviusion ([@Reviusion](https://github.com/Reviusion)).
- **Fork maintainer**: NazarHK ([@nazarhktwitch](https://github.com/nazarhktwitch)) - Vulkan backend, uninstaller, active updates.

### License

MIT - see [`LICENSE`](LICENSE). You may use, modify, redistribute, and even sell this code, as long as the copyright notice is preserved.

ANGLE binaries are licensed under the [BSD 3-Clause license](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/LICENSE) and are not part of this repository's source - they're bundled in releases for convenience only.

### Disclaimer

This is a **third-party** modification. Use at your own risk. **Always back up your `Geometry Dash` folder** before installing. The author is not affiliated with RobTop Games and is not responsible for save corruption, account bans (none observed in testing, but theoretically possible), or any other adverse effects.

---

## Russian

### О этом форке

Это **активный форк** оригинального проекта [ReviANGLE](https://github.com/Reviusion/ReviANGLE) с **одновременной поддержкой обоих бэкендов: DirectX 11 и Vulkan**. Поддерживается на [@nazarhktwitch/ReviANGLE](https://github.com/nazarhktwitch/ReviANGLE). Оригинальный репозиторий отслеживается - обновления upstream будут интегрироваться по мере их выхода.

**Ключевые отличия от оригинала:**
- **Внутриигровой оверлей-конфигуратор**: Нажмите **`Alt + Home`** прямо в игре для настройки параметров в реальном времени без перезапуска
- **Vulkan-бэкенд**, собранный из исходников ANGLE специально для этого проекта (кастомная компиляция DLL)
- **Совместимость с модами**: Восстановлены экспорты функций и атрибутный стек legacy OpenGL 1.1 для безупречной отрисовки оверлеев
- **Поддержка обоих бэкендов**: Выбор между DirectX 11 и Vulkan для каждой сборки
- **Автоматизированный CI/CD**: Чистая сборка из исходников через GitHub Actions и локальный скрипт упаковки `build_release.ps1`
- **Деинсталлятор**: Деинсталлятор с автоопределением игры

---

### Что это?

ReviANGLE - это мод производительности для **Geometry Dash 2.2**, который заменяет `opengl32.dll` игры на кастомный прокси. Прокси:

1. **Транслирует OpenGL → DirectX 11 или Vulkan** через [Google ANGLE](https://chromium.googlesource.com/angle/angle) - выберите бэкенд, подходящий для вашей видеокарты:
   - **DirectX 11** (рекомендуется по умолчанию): Высокая совместимость, аппаратная поддержка DXGI Allow Tearing (Flip Model) и максимальная пиковая пропускная способность в нелимитированных бенчмарках на видеокартах NVIDIA.
   - **Vulkan**: Современный низкоуровневый бэкенд для Linux/Proton и современных видеокарт. Производительность зависит от конфигурации системы: Vulkan обходит конфликты с хуками оверлеев DXGI (часто обеспечивая более плавный вывод кадров без микрофризов), хотя в пустых нелимитированных сценах на Windows может показывать более низкий пиковый FPS по сравнению с DirectX 11.
2. Добавляет **84 низкоуровневых модуля оптимизации производительности**, встраиваемых в критический путь ANGLE:
   - Дедупликация состояния GL (пропуск 30–50 % избыточных вызовов cocos2d-x)
   - Высокоточный frame pacing (без активного ожидания на CPU)
   - Низколатентный вывод кадров через DXGI (очередь в 1 кадр)
   - Опциональный рендеринг в половинном разрешении с линейным масштабированием (~30–50 % прироста производительности GPU на слабых видеокартах)
   - Опциональный пропуск Present для кадров в состоянии простоя (экономия ресурсов GPU в меню)
   - Профиль драйвера NVAPI (PSTATE=P0, максимальная производительность, отключение драйверного VSync)
   - Фиксация working set, чтобы Windows не выгружала часто используемые данные в файл подкачки
   - Повышение приоритета потока GPU
   - И ещё 40+ оптимизаций - см. [`docs/CONFIG_REFERENCE.md`](docs/CONFIG_REFERENCE.md)
3. Поставляется с **ReviANGLE Studio** - отдельным GUI-конфигуратором с двуязычными (EN/RU) описаниями для каждой опции.

### Зачем?

Ванильная GD создавалась для OpenGL на оборудовании, которому уже **15+ лет**. На слабых ноутбуках (Intel HD / GT 630M и т. д.) она может фризить, ограничиваться 60 FPS и тратить огромное количество ресурсов CPU на избыточные вызовы драйвера. ReviANGLE оптимизирует этот графический конвейер.

### Быстрая установка

1. Перейдите в [**Релизы**](https://github.com/nazarhktwitch/ReviANGLE/releases) и скачайте:
   - **Сборку DirectX 11** (по умолчанию): `ReviANGLE-vX.Y.Z-DX11.zip`
   - **Сборку Vulkan** (для современных видеокарт): `ReviANGLE-vX.Y.Z-Vulkan.zip`
2. **Сделайте резервную копию** папки `Geometry Dash` (или хотя бы оригинального `opengl32.dll`, если он существует).
3. Распакуйте ZIP-архив в папку с установленной Geometry Dash (где находится `GeometryDash.exe`).

**Сборка DirectX 11** содержит:
```text
Geometry Dash/
├── GeometryDash.exe
├── opengl32.dll              ← от ReviANGLE
├── libEGL.dll                ← от ANGLE
├── libGLESv2.dll             ← от ANGLE
├── d3dcompiler_47.dll        ← компилятор DirectX 11
├── angle_config.ini          ← конфигурационный файл (можно редактировать)
├── gd-angle-editor.exe       ← GUI-конфигуратор
└── ReviANGLE-Uninstall.exe   ← деинсталлятор
```

**Сборка Vulkan** содержит:
```text
Geometry Dash/
├── GeometryDash.exe
├── opengl32.dll              ← от ReviANGLE
├── libEGL.dll                ← от ANGLE
├── libGLESv2.dll             ← от ANGLE
├── vulkan-1.dll              ← среда выполнения Vulkan
├── angle_config.ini          ← конфигурационный файл (можно редактировать)
├── gd-angle-editor.exe       ← GUI-конфигуратор
└── ReviANGLE-Uninstall.exe   ← деинсталлятор
```

4. Запустите GD. Если всё работает правильно, рядом с `.exe` появится файл `angle_log.txt`.

**Не можете определиться?** Начните с **DirectX 11** - это самый стабильный вариант по умолчанию. Если у вас RTX или современная видеокарта Radeon и вы хотите максимальную производительность, попробуйте Vulkan.

#### Linux и Steam Deck (Proton / Wine)

ReviANGLE поддерживает Linux и Steam Deck через Proton:

1. Распакуйте файлы ZIP-архива в папку GD рядом с `GeometryDash.exe`.
2. В Steam откройте **Geometry Dash → Свойства → Параметры запуска** и введите:
   ```bash
   WINEDLLOVERRIDES="opengl32=n,b" %command%
   ```

### Настройка

Запустите **`gd-angle-editor.exe`**, чтобы открыть графический интерфейс:

- Двуязычные описания (на английском и русском языках) для каждой опции
- Выбор бэкенда (DirectX 11 / Vulkan / DirectX 9)
- Сохранение комментариев и структуры секций при записи (безопасное сохранение с сохранением исходной структуры)
- Сброс к настройкам по умолчанию в один клик применяет **пресет с оптимальными ощущениями от игры** для протестированного оборудования

Или редактируйте `angle_config.ini` напрямую - это обычный текстовый файл с подробными двуязычными комментариями.

### Сборка из исходников

См. [`docs/BUILDING.md`](docs/BUILDING.md). Краткая инструкция:

```powershell
# Требования: Visual Studio 2022 (компонент разработки на C++), CMake 3.20+
git clone https://github.com/nazarhktwitch/ReviANGLE.git
cd ReviANGLE

# Автоматическая упаковка релиза (сборки DX11 и Vulkan):
# Замените "v1.1.0" на нужную версию
.\build_release.ps1 -Version "v1.1.0"

# Или ручная сборка через CMake (например, DX11):
cmake -B build_dx11 -A x64 -DREVIANGLE_BACKEND_D3D11=ON -DREVIANGLE_BACKEND_VULKAN=OFF
cmake --build build_dx11 --config Release
```

Скомпилированные файлы находятся в `build_dx11\dll\Release\` (`opengl32.dll`, `gd-angle-editor.exe`) и `build_dx11\bin\Release\` (`ReviANGLE-Uninstall.exe`).

Предварительно собранные зависимости ANGLE находятся в `deps/dx11` и `deps/vulkan`.

### Совместимость

| Совместимость | Статус |
|------------|--------|
| Geometry Dash 2.2 (Steam, standalone) | ✅ протестировано |
| MegaHack | ✅ протестировано (совместимо) |
| Eclipse Menu | ✅ протестировано (совместимо) |
| Linux / Steam Deck | ✅ Поддерживается через Proton / Wine |
| macOS | ❌ Слишком много работы... |

### Благодарности и авторы

- Команда **ANGLE** в Google - за библиотеку трансляции GLES → D3D.
- Авторы **cocos2d-x** - за движок, лежащий в основе Geometry Dash.
- **RobTop Games** - за саму Geometry Dash (данный мод не связан с разработчиком игры).
- **Dear ImGui** - используется для графического интерфейса конфигуратора.
- **Оригинальный проект**: Reviusion ([@Reviusion](https://github.com/Reviusion)).
- **Мейнтейнер форка**: NazarHK ([@nazarhktwitch](https://github.com/nazarhktwitch)) - бэкенд Vulkan, деинсталлятор и активная поддержка проекта.

### Лицензия

MIT - см. [`LICENSE`](LICENSE). Вы можете использовать, изменять, распространять и даже продавать этот код при условии сохранения уведомления об авторских правах.

Бинарные файлы ANGLE распространяются по [лицензии BSD 3-Clause](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/LICENSE) и не входят в исходный код этого репозитория - они включены в релизы исключительно для удобства.

### Отказ от ответственности

Это **сторонняя** модификация. Используйте её на свой страх и риск. **Всегда делайте резервную копию папки `Geometry Dash`** перед установкой. Автор не связан с RobTop Games и не несёт ответственности за повреждение сохранений, блокировки аккаунтов (при тестировании не зафиксированы, но теоретически возможны) или любые другие негативные последствия.

---

<div align="center">

**Original project** by Reviusion. **Fork** maintained by NazarHK at [@nazarhktwitch/ReviANGLE](https://github.com/nazarhktwitch/ReviANGLE)

Licensed under MIT - See [LICENSE](LICENSE) for details.

Made with love by Reviusion and NazarHK ❤

</div>
