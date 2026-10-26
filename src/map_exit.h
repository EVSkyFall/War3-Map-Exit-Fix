#pragma once
#include <windows.h>
struct MapExitSites { BYTE* hook; BYTE* finish; };
bool FindMapExitSites(BYTE* image, MapExitSites* output);
