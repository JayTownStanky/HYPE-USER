#include "../feature.h"
#include "../offsets.hpp"
#include <imgui.h>
#include <Windows.h>
#include <chrono>
#include <memory>

class TriggerbotFeature : public Feature {
public:
    TriggerbotFeature() {
        m_name = "Triggerbot";
        m_hotkey = VK_XBUTTON1;  // Mouse4 by default
    }

    // Settings
    int delay_ms = 50;
    bool team_check = true;
    float max_distance = 500.0f;
    float head_hitbox_radius = 10.0f;
    float body_hitbox_radius = 20.0f;

    // State
    bool is_triggered = false;
    std::chrono::steady_clock::time_point last_shot_time;

    void tick(GameState& state, HvMemory& mem) override {
        (void)mem;
        is_triggered = false;

        if (!state.ready()) return;

        // Only active while hotkey is held
        if (!(GetAsyncKeyState(m_hotkey) & 0x8000)) return;

        const auto& local = state.local_player();
        if (!local.valid() || !local.alive()) return;

        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        float center_x = screen_w / 2.0f;
        float center_y = screen_h / 2.0f;

        const auto& view = state.view_matrix();

        for (const auto& ent : state.entities()) {
            if (!ent.alive()) continue;
            if (ent.address == local.address) continue;
            if (team_check && ent.team == local.team) continue;

            float dist = ent.distance_to(local.position);
            if (max_distance > 0 && dist > max_distance) continue;

            // Check head hitbox
            Vec2 head_screen;
            if (view.world_to_screen(ent.head_position, head_screen, screen_w, screen_h)) {
                float dx = head_screen.x - center_x;
                float dy = head_screen.y - center_y;
                float dist_to_crosshair = std::sqrt(dx * dx + dy * dy);

                // Scale hitbox by distance
                float scaled_head_radius = head_hitbox_radius * (1000.0f / dist);
                if (scaled_head_radius > 50.0f) scaled_head_radius = 50.0f;

                if (dist_to_crosshair < scaled_head_radius) {
                    is_triggered = true;
                    try_shoot();
                    return;
                }
            }

            // Check body hitbox
            Vec3 body_pos = ent.position;
            body_pos.z += 40.0f;  // Approximate chest height

            Vec2 body_screen;
            if (view.world_to_screen(body_pos, body_screen, screen_w, screen_h)) {
                float dx = body_screen.x - center_x;
                float dy = body_screen.y - center_y;
                float dist_to_crosshair = std::sqrt(dx * dx + dy * dy);

                float scaled_body_radius = body_hitbox_radius * (1000.0f / dist);
                if (scaled_body_radius > 80.0f) scaled_body_radius = 80.0f;

                if (dist_to_crosshair < scaled_body_radius) {
                    is_triggered = true;
                    try_shoot();
                    return;
                }
            }
        }
    }

    void try_shoot() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shot_time);

        if (elapsed.count() < delay_ms) return;

        // Send mouse click
        INPUT inputs[2] = {};

        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

        SendInput(2, inputs, sizeof(INPUT));
        last_shot_time = now;
    }

    void render(ImDrawList* draw, float screen_w, float screen_h, const GameState& state) override {
        (void)state;

        if (!is_triggered) return;

        // Draw small indicator when triggered
        float center_x = screen_w / 2.0f;
        float center_y = screen_h / 2.0f;

        draw->AddCircle(ImVec2(center_x, center_y), 5.0f, IM_COL32(255, 50, 50, 255), 8, 2.0f);
    }

    void render_menu() override {
        ImGui::SliderInt("Delay (ms)", &delay_ms, 0, 200);
        ImGui::Checkbox("Team Check", &team_check);
        ImGui::SliderFloat("Max Distance", &max_distance, 0.0f, 2000.0f, "%.0f");
        ImGui::SliderFloat("Head Hitbox", &head_hitbox_radius, 5.0f, 30.0f, "%.0f");
        ImGui::SliderFloat("Body Hitbox", &body_hitbox_radius, 10.0f, 50.0f, "%.0f");
    }
};

std::unique_ptr<Feature> create_triggerbot_feature() {
    return std::make_unique<TriggerbotFeature>();
}
