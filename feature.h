#ifndef FEATURE_H
#define FEATURE_H

#include "game_sdk.h"
#include "mem.h"
#include <imgui.h>

struct Feature {
    const char* m_name = "Unknown";
    bool m_enabled = false;
    int m_hotkey = 0;

    virtual ~Feature() = default;

    // Called each frame when enabled
    virtual void tick(GameState& state, HvMemory& mem) = 0;

    // Called each frame when enabled to render visuals
    virtual void render(ImDrawList* draw, float screen_w, float screen_h, const GameState& state) = 0;

    // Called to render feature's menu options
    virtual void render_menu() = 0;
};

#endif // FEATURE_H
