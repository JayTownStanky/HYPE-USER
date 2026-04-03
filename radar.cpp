#include "../feature.h"
#include "../offsets.hpp"
#include <imgui.h>
#include <cmath>
#include <memory>

class RadarFeature : public Feature {
public:
    RadarFeature() {
        m_name = "Radar";
    }

    // Settings
    float radar_size = 200.0f;
    float zoom = 0.05f;  // Smaller = more zoomed out
    bool show_friendly = true;
    float dot_size = 4.0f;
    ImU32 enemy_color = IM_COL32(255, 50, 50, 255);
    ImU32 friendly_color = IM_COL32(50, 150, 255, 255);
    ImU32 local_color = IM_COL32(50, 255, 50, 255);
    ImU32 bg_color = IM_COL32(0, 0, 0, 150);

    void tick(GameState& state, HvMemory& mem) override {
        (void)state;
        (void)mem;
    }

    void render(ImDrawList* draw, float screen_w, float screen_h, const GameState& state) override {
        (void)screen_h;

        if (!state.ready()) return;

        const auto& local = state.local_player();
        if (!local.valid()) return;

        // Radar position (top-right corner)
        float padding = 20.0f;
        float radar_x = screen_w - radar_size - padding;
        float radar_y = padding;
        float center_x = radar_x + radar_size / 2.0f;
        float center_y = radar_y + radar_size / 2.0f;

        // Draw background
        draw->AddRectFilled(
            ImVec2(radar_x, radar_y),
            ImVec2(radar_x + radar_size, radar_y + radar_size),
            bg_color
        );
        draw->AddRect(
            ImVec2(radar_x, radar_y),
            ImVec2(radar_x + radar_size, radar_y + radar_size),
            IM_COL32(100, 100, 100, 255)
        );

        // Draw crosshair
        draw->AddLine(
            ImVec2(center_x - 5, center_y),
            ImVec2(center_x + 5, center_y),
            IM_COL32(100, 100, 100, 255)
        );
        draw->AddLine(
            ImVec2(center_x, center_y - 5),
            ImVec2(center_x, center_y + 5),
            IM_COL32(100, 100, 100, 255)
        );

        // Draw local player (center dot)
        draw->AddCircleFilled(ImVec2(center_x, center_y), dot_size, local_color);

        // Get local player view angle (approximate from position delta or use actual angle if available)
        // For now, assume facing +Y direction (yaw = 0)
        float local_yaw = 0.0f;  // In radians, would need to read from game

        for (const auto& ent : state.entities()) {
            if (!ent.alive()) continue;
            if (ent.address == local.address) continue;

            bool is_enemy = ent.team != local.team;
            if (!show_friendly && !is_enemy) continue;

            // Calculate relative position
            float dx = ent.position.x - local.position.x;
            float dy = ent.position.y - local.position.y;

            // Rotate relative to local view angle
            float cos_yaw = std::cos(-local_yaw);
            float sin_yaw = std::sin(-local_yaw);
            float rotated_x = dx * cos_yaw - dy * sin_yaw;
            float rotated_y = dx * sin_yaw + dy * cos_yaw;

            // Scale to radar
            float radar_point_x = center_x + rotated_x * zoom;
            float radar_point_y = center_y - rotated_y * zoom;  // Invert Y for screen coords

            // Clamp to radar bounds
            float half_size = radar_size / 2.0f - dot_size;
            if (radar_point_x < center_x - half_size) radar_point_x = center_x - half_size;
            if (radar_point_x > center_x + half_size) radar_point_x = center_x + half_size;
            if (radar_point_y < center_y - half_size) radar_point_y = center_y - half_size;
            if (radar_point_y > center_y + half_size) radar_point_y = center_y + half_size;

            ImU32 color = is_enemy ? enemy_color : friendly_color;
            draw->AddCircleFilled(ImVec2(radar_point_x, radar_point_y), dot_size, color);
        }
    }

    void render_menu() override {
        ImGui::SliderFloat("Size", &radar_size, 100.0f, 400.0f, "%.0f");
        ImGui::SliderFloat("Zoom", &zoom, 0.01f, 0.2f, "%.3f");
        ImGui::SliderFloat("Dot Size", &dot_size, 2.0f, 10.0f, "%.1f");
        ImGui::Checkbox("Show Friendly", &show_friendly);
    }
};

std::unique_ptr<Feature> create_radar_feature() {
    return std::make_unique<RadarFeature>();
}
