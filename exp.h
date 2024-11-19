#pragma once
#include <Windows.h>
void LoadExports_version(HMODULE originaldll);
void LoadExports_winmm(HMODULE originaldll);
void LoadExports_winhttp(HMODULE originaldll);