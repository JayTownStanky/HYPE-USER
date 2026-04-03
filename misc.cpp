#include "../feature.h"
#include "../offsets.hpp"
#include <imgui.h>
#include <Windows.h>
#include <memory>

class MiscFeature : public Feature {
public:
    MiscFeature() {
        m_name = "Misc";
    }

    // Settings
    bool bhop_enabled = false;
    bool no_recoil_enabled = false;
    float recoil_compensation = 2.0f;

    // State
    Vec3 last_punch;
    bool was_grounded = false;

    void tick(GameState& state, HvMemory& mem) override {
        if (!state.ready()) return;

        const auto& local = state.local_player();
        if (!local.valid() || !local.alive()) return;

        // Bunny hop
        if (bhop_enabled) {
            tick_bhop(state, mem);
        }

        // Recoil compensation
        if (no_recoil_enabled) {
            tick_recoil(state, mem);
        }
    }

    void tick_bhop(GameState& state, HvMemory& mem) {
        // Only when space is held
        if (!(GetAsyncKeyState(VK_SPACE) & 0x8000)) {
            was_grounded = false;
            return;
        }

        // Read flags to check if on ground
        auto flags = mem.read<uint32_t>(state.local_player().address + OFF_FLAGS);
        if (!flags) return;

        bool on_ground = (*flags & (1 << 0)) != 0;  // FL_ONGROUND = 1

        // Jump when landing (edge trigger)
        if (on_ground && !was_grounded) {
            // Send jump input
            INPUT inputs[2] = {};

            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_SPACE;
            inputs[0].ki.dwFlags = 0;

            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = VK_SPACE;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

            SendInput(2, inputs, sizeof(INPUT));
        }

        was_grounded = on_ground;
    }

    void tick_recoil(GameState& state, HvMemory& mem) {
        // Only when shooting (mouse1 held)
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            last_punch = Vec3();
            return;
        }

        // Read punch angles
        auto punch = mem.read<Vec3>(state.local_player().address + OFF_PUNCH_ANGLES);
        if (!punch) return;

        Vec3 current_punch = *punch;

        // Calculate delta from last frame
        Vec3 delta;
        delta.x = current_punch.x - last_punch.x;
        delta.y = current_punch.y - last_punch.y;

        // Compensate mouse movement
        if (delta.x != 0.0f || delta.y != 0.0f) {
            // Convert angles to pixels (approximate)
            float pixels_per_degree = GetSystemMetrics(SM_CXSCREEN) / 90.0f * 0.022f;

            int dx = (int)(-delta.y * recoil_compensation * pixels_per_degree);
            int dy = (int)(delta.x * recoil_compensation * pixels_per_degree);

            if (dx != 0 || dy != 0) {
                INPUT input = {};
                input.type = INPUT_MOUSE;
                input.mi.dwFlags = MOUSEEVENTF_MOVE;
                input.mi.dx = dx;
                input.mi.dy = dy;
                SendInput(1, &input, sizeof(INPUT));
            }
        }

        last_punch = current_punch;
    }

    void render(ImDrawList* draw, float screen_w, float screen_h, const GameState& state) override {
        (void)draw;
        (void)screen_w;
        (void)screen_h;
        (void)state;
        // No render for misc features
    }

    void render_menu() override {
        ImGui::Checkbox("Bunny Hop", &bhop_enabled);
        ImGui::Checkbox("No Recoil", &no_recoil_enabled);

        if (no_recoil_enabled) {
            ImGui::SliderFloat("Recoil Compensation", &recoil_compensation, 1.0f, 3.0f, "%.1f");
        }
    }
};

std::unique_ptr<Feature> create_misc_feature() {
    return std::make_unique<MiscFeature>();
}
