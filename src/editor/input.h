#pragma once

#include "editor.h"

constexpr float MINIMUM_MOVEMENT_SPEED{ 0.1f };

void ProcessViewportInput(Editor* editor);

void ProcessTreeViewInput(Editor* editor);

void ProcessFileBrowserInput(Editor* editor);
