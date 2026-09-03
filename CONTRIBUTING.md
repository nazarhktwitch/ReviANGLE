# Contributing to ReviANGLE

Thanks for considering a contribution! ReviANGLE is a small project with a single primary maintainer, but bug reports, profiling data, and pull requests are very welcome.

## Quick checklist

- **Bug report**: open an [Issue](https://github.com/Reviusion/ReviANGLE/issues/new/choose) using the template. Always attach `angle_log.txt`.
- **Feature / optimization idea**: open an Issue first to discuss before coding - keeps both sides from wasting time.
- **Pull request**: see below.

## Reporting bugs effectively

The single most useful thing you can attach is the **`angle_log.txt`** file (lives next to `GeometryDash.exe` after the mod runs once). It contains:
- Hardware detection results (GPU forced, NVAPI status, monitor refresh, …)
- Which modules apply'd successfully
- Per-module diagnostic logs
- Driver version (Nvidia / D3D11 / etc.)

Include also:
- ReviANGLE version (from the release zip name)
- Geometry Dash version
- Whether you have **other mods** installed (Eclipse, Geode, MegaHack…)
- Steps to reproduce - *which level / menu does the bug occur on?*

## Pull requests

1. Open an Issue first if it's a non-trivial change.
2. Fork → branch off `main` → commit with **descriptive messages**.
3. Match the existing style:
   - **C++17**, MSVC `/Wall` clean.
   - 4-space indent, no tabs.
   - One module per `boost_*.cpp` file - keep them small and focused.
   - Comments must explain **why** (not what), in English.
   - For dedup hooks: use `thread_local` storage, no heap allocations on the hot path.
4. **Test on a clean GD install**:
   - Vanilla 2.2 (no other mods)
   - At least one menu transition + one full level run + one editor open
   - Check `angle_log.txt` for new errors
5. Document config changes in:
   - `src/config.hpp` - add the field
   - `src/config.cpp` - parse the key
   - `config_editor/schema.cpp` - add bilingual UI metadata
   - `examples/angle_config.default-safe.ini` - add bilingual comment + default
6. Open the PR - explain *what changed* and *measurable impact* (FPS numbers, before/after frame times, etc.).

## Performance claims

Any optimization PR claiming an FPS gain should include:
- **Test scene** (level name + ID, or menu, or scene description)
- **Hardware** (CPU, GPU, RAM, OS build)
- **Method** (uncapped FPS? pacer on? what target?)
- **Numbers** - at minimum avg FPS and 1 % low, ideally over 5+ runs

Numbers like "+10 % FPS" without context are not useful and will be asked for clarification.

## Style - what we **don't** do

- No emojis in code (keep them in user-facing docs only).
- No `using namespace std;` at file scope.
- No exceptions in hot paths - the GL/EGL hooks must be `noexcept`.
- No global mutable state outside `thread_local` caches.
- No platform-specific code outside Windows (this is a Windows-only project).

## Code of conduct

Be polite. Disagree on technical merit. Don't make it personal. That's all.

## License of contributions

By submitting a PR, you agree your changes will be licensed under the same MIT license that covers the rest of the project.
