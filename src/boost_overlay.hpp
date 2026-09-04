#pragma once

#include <windows.h>

namespace boost_overlay {

// Apply / initialize overlay hooks
void apply();

// Render loop called inside wglSwapBuffers
void render(HDC hdc);

// Check if overlay is currently open
bool isVisible();

// Shutdown overlay resources on detach
void shutdown();

} // namespace boost_overlay
