#pragma once

#define DEBUG_MESSAGE(FORMAT, ...) DebugMessage(FORMAT, __VA_ARGS__)


void DebugMessage(const char* pszFormat, ...);

void ErrorMessage(const char* pszFormat, ...);