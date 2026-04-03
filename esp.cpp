#include "../feature.h"
#include "../offsets.hpp"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <memory>

class EspFeature : public Feature {
public:
    EspFeature() {
        m_name = "ESP";
    }

    // Box styles
    enum class BoxStyle { None = 0, Box2D, Corner };
    enum class HealthStyle { Left = 0, Top, Bottom, Seer };

    // Per-target-type settings
    struct TargetSettings {
        bool enabled = true;
        float max_distance = 500.0f;
        bool show_box = true;
        bool show_health = true;
        bool show_distance = true;
        bool show_name = true;
        bool show_weapon = false;
        bool show_skeleton = true;
        bool visibility_check = false;
        BoxStyle box_style = BoxStyle::Corner;
        HealthStyle health_style = HealthStyle::Left;
        ImVec4 visible_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        ImVec4 invisible_color = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
        ImVec4 skeleton_visible = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        ImVec4 skeleton_invisible = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        float box_thickness = 1.5f;
        float skeleton_thickness = 1.5f;
    };

    // Animation settings
    struct {
        bool enabled = true;
        float fade_start = 100.0f;
        float fade_end = 20.0f;
        float scale = 1.0f;
    } animation;

    // Off-screen indicator
    struct {
        bool enabled = true;
        float max_distance = 200.0f;
        bool show_distance = true;
        bool enemy_only = true;
        float radius = 0.4f;
        float arrow_size = 12.0f;
        ImVec4 color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
    } offscreen;

    TargetSettings enemies;
    TargetSettings teammates;
    bool team_check = true;
    int current_tab = 0;

    // Bone indices for skeleton
    static constexpr int BONE_HEAD = 8;
    static constexpr int BONE_NECK = 7;
    static constexpr int BONE_SPINE_UPPER = 5;
    static constexpr int BONE_SPINE_LOWER = 4;
    static constexpr int BONE_PELVIS = 0;
    static constexpr int BONE_SHOULDER_L = 11;
    static constexpr int BONE_SHOULDER_R = 38;
    static constexpr int BONE_ELBOW_L = 12;
    static constexpr int BONE_ELBOW_R = 39;
    static constexpr int BONE_HAND_L = 13;
    static constexpr int BONE_HAND_R = 40;
    static constexpr int BONE_HIP_L = 63;
    static constexpr int BONE_HIP_R = 58;
    static constexpr int BONE_KNEE_L = 64;
    static constexpr int BONE_KNEE_R = 59;
    static constexpr int BONE_FOOT_L = 65;
    static constexpr int BONE_FOOT_R = 60;

    void tick(GameState& state, HvMemory& mem) override {
        (void)state;
        (void)mem;
    }

    void render(ImDrawList* draw, float screen_w, float screen_h, const GameState& state) override {
        if (!state.ready()) return;

        const auto& local = state.local_player();
        if (!local.valid()) return;

        const auto& view = state.view_matrix();

        for (const auto& ent : state.entities()) {
            if (!ent.alive()) continue;
            if (ent.address == local.address) continue;

            bool is_enemy = ent.team != local.team;

            // Select settings based on team
            const TargetSettings& settings = is_enemy ? enemies : teammates;
            if (!settings.enabled) continue;
            if (team_check && !is_enemy) continue;

            float dist = ent.distance_to(local.position);
            if (settings.max_distance > 0 && dist > settings.max_distance) continue;

            // Calculate alpha based on animation settings
            float alpha = 1.0f;
            if (animation.enabled && dist > animation.fade_end) {
                alpha = 1.0f - std::clamp((dist - animation.fade_end) / (animation.fade_start - animation.fade_end), 0.0f, 1.0f);
                alpha = std::max(alpha, 0.3f);
            }

            // World to screen
            Vec2 head_screen, feet_screen;
            if (!view.world_to_screen(ent.head_position, head_screen, (int)screen_w, (int)screen_h)) {
                // Off-screen indicator
                if (offscreen.enabled && is_enemy) {
                    draw_offscreen_indicator(draw, screen_w, screen_h, ent, local, view);
                }
                continue;
            }
            if (!view.world_to_screen(ent.position, feet_screen, (int)screen_w, (int)screen_h)) continue;

            // Determine visibility (placeholder - would need raycast)
            bool is_visible = true; // TODO: implement visibility check

            // Select colors based on visibility
            ImVec4 box_color = is_visible ? settings.visible_color : settings.invisible_color;
            ImVec4 skel_color = is_visible ? settings.skeleton_visible : settings.skeleton_invisible;

            // Apply alpha
            box_color.w *= alpha;
            skel_color.w *= alpha;

            // Calculate box dimensions
            float box_height = feet_screen.y - head_screen.y;
            float box_width = box_height * 0.5f;
            float box_left = head_screen.x - box_width * 0.5f;
            float box_right = head_screen.x + box_width * 0.5f;
            float box_top = head_screen.y;
            float box_bottom = feet_screen.y;

            ImU32 box_col32 = ImGui::ColorConvertFloat4ToU32(box_color);

            // Draw box
            if (settings.show_box) {
                switch (settings.box_style) {
                    case BoxStyle::Box2D:
                        draw_box_2d(draw, box_left, box_top, box_right, box_bottom, box_col32, settings.box_thickness);
                        break;
                    case BoxStyle::Corner:
                        draw_corner_box(draw, box_left, box_top, box_right, box_bottom, box_col32, settings.box_thickness, box_height);
                        break;
                    default:
                        break;
                }
            }

            // Draw skeleton
            if (settings.show_skeleton) {
                draw_skeleton(draw, ent, view, (int)screen_w, (int)screen_h,
                             ImGui::ColorConvertFloat4ToU32(skel_color), settings.skeleton_thickness);
            }

            // Draw health bar
            if (settings.show_health && ent.max_health > 0) {
                draw_health_bar(draw, box_left, box_top, box_right, box_bottom,
                               ent.health, ent.max_health, settings.health_style, alpha);
            }

            // Draw info text
            float text_y = box_top - 2;

            if (settings.show_name) {
                char name_text[32];
                snprintf(name_text, sizeof(name_text), "Player [%d]", ent.team);
                draw_text_with_shadow(draw, name_text, head_screen.x, text_y, alpha);
                text_y -= 14;
            }

            if (settings.show_distance) {
                char dist_text[32];
                snprintf(dist_text, sizeof(dist_text), "%.0fm", dist);
                draw_text_with_shadow(draw, dist_text, head_screen.x, box_bottom + 2, alpha, false);
            }
        }
    }

    void draw_box_2d(ImDrawList* draw, float left, float top, float right, float bottom,
                     ImU32 color, float thickness) {
        draw->AddRect(ImVec2(left, top), ImVec2(right, bottom), color, 0.0f, 0, thickness);
    }

    void draw_corner_box(ImDrawList* draw, float left, float top, float right, float bottom,
                         ImU32 color, float thickness, float height) {
        float corner_len = height * 0.25f;
        corner_len = std::clamp(corner_len, 5.0f, 25.0f);

        // Top-left
        draw->AddLine(ImVec2(left, top), ImVec2(left + corner_len, top), color, thickness);
        draw->AddLine(ImVec2(left, top), ImVec2(left, top + corner_len), color, thickness);

        // Top-right
        draw->AddLine(ImVec2(right, top), ImVec2(right - corner_len, top), color, thickness);
        draw->AddLine(ImVec2(right, top), ImVec2(right, top + corner_len), color, thickness);

        // Bottom-left
        draw->AddLine(ImVec2(left, bottom), ImVec2(left + corner_len, bottom), color, thickness);
        draw->AddLine(ImVec2(left, bottom), ImVec2(left, bottom - corner_len), color, thickness);

        // Bottom-right
        draw->AddLine(ImVec2(right, bottom), ImVec2(right - corner_len, bottom), color, thickness);
        draw->AddLine(ImVec2(right, bottom), ImVec2(right, bottom - corner_len), color, thickness);
    }

    void draw_skeleton(ImDrawList* draw, const Entity& ent, const ViewMatrix& view,
                       int screen_w, int screen_h, ImU32 color, float thickness) {
        // Simplified skeleton - just spine and limbs approximation
        Vec3 head = ent.head_position;
        Vec3 chest = ent.position;
        chest.z += 50.0f;
        Vec3 pelvis = ent.position;
        pelvis.z += 30.0f;

        Vec2 head_s, chest_s, pelvis_s, feet_s;
        if (!view.world_to_screen(head, head_s, screen_w, screen_h)) return;
        if (!view.world_to_screen(chest, chest_s, screen_w, screen_h)) return;
        if (!view.world_to_screen(pelvis, pelvis_s, screen_w, screen_h)) return;
        if (!view.world_to_screen(ent.position, feet_s, screen_w, screen_h)) return;

        // Spine
        draw->AddLine(ImVec2(head_s.x, head_s.y), ImVec2(chest_s.x, chest_s.y), color, thickness);
        draw->AddLine(ImVec2(chest_s.x, chest_s.y), ImVec2(pelvis_s.x, pelvis_s.y), color, thickness);

        // Approximate arms (offset from chest)
        float arm_offset = (feet_s.y - head_s.y) * 0.15f;
        draw->AddLine(ImVec2(chest_s.x - arm_offset, chest_s.y),
                     ImVec2(chest_s.x + arm_offset, chest_s.y), color, thickness);
        draw->AddLine(ImVec2(chest_s.x - arm_offset, chest_s.y),
                     ImVec2(chest_s.x - arm_offset * 1.5f, chest_s.y + arm_offset), color, thickness);
        draw->AddLine(ImVec2(chest_s.x + arm_offset, chest_s.y),
                     ImVec2(chest_s.x + arm_offset * 1.5f, chest_s.y + arm_offset), color, thickness);

        // Approximate legs
        float leg_offset = (feet_s.y - head_s.y) * 0.1f;
        draw->AddLine(ImVec2(pelvis_s.x, pelvis_s.y), ImVec2(feet_s.x - leg_offset, feet_s.y), color, thickness);
        draw->AddLine(ImVec2(pelvis_s.x, pelvis_s.y), ImVec2(feet_s.x + leg_offset, feet_s.y), color, thickness);
    }

    void draw_health_bar(ImDrawList* draw, float left, float top, float right, float bottom,
                         int health, int max_health, HealthStyle style, float alpha) {
        float health_pct = std::clamp((float)health / (float)max_health, 0.0f, 1.0f);

        // Health color gradient
        ImU32 health_color = IM_COL32(
            (int)(255 * (1.0f - health_pct)),
            (int)(255 * health_pct),
            0, (int)(255 * alpha)
        );
        ImU32 bg_color = IM_COL32(0, 0, 0, (int)(150 * alpha));

        float bar_thickness = 4.0f;

        switch (style) {
            case HealthStyle::Left: {
                float bar_height = (bottom - top) * health_pct;
                draw->AddRectFilled(ImVec2(left - bar_thickness - 2, top),
                                   ImVec2(left - 2, bottom), bg_color);
                draw->AddRectFilled(ImVec2(left - bar_thickness - 2, bottom - bar_height),
                                   ImVec2(left - 2, bottom), health_color);
                break;
            }
            case HealthStyle::Top: {
                float bar_width = (right - left) * health_pct;
                draw->AddRectFilled(ImVec2(left, top - bar_thickness - 2),
                                   ImVec2(right, top - 2), bg_color);
                draw->AddRectFilled(ImVec2(left, top - bar_thickness - 2),
                                   ImVec2(left + bar_width, top - 2), health_color);
                break;
            }
            case HealthStyle::Bottom: {
                float bar_width = (right - left) * health_pct;
                draw->AddRectFilled(ImVec2(left, bottom + 2),
                                   ImVec2(right, bottom + bar_thickness + 2), bg_color);
                draw->AddRectFilled(ImVec2(left, bottom + 2),
                                   ImVec2(left + bar_width, bottom + bar_thickness + 2), health_color);
                break;
            }
            case HealthStyle::Seer: {
                // Seer-style segmented health bar
                float bar_width = right - left;
                int segments = 4;
                float seg_width = bar_width / segments;
                float seg_gap = 2.0f;

                for (int i = 0; i < segments; i++) {
                    float seg_left = left + i * seg_width + (i > 0 ? seg_gap / 2 : 0);
                    float seg_right = left + (i + 1) * seg_width - (i < segments - 1 ? seg_gap / 2 : 0);

                    float seg_health = std::clamp((health_pct - (float)i / segments) * segments, 0.0f, 1.0f);

                    draw->AddRectFilled(ImVec2(seg_left, bottom + 2),
                                       ImVec2(seg_right, bottom + bar_thickness + 2), bg_color);
                    if (seg_health > 0) {
                        draw->AddRectFilled(ImVec2(seg_left, bottom + 2),
                                           ImVec2(seg_left + (seg_right - seg_left) * seg_health, bottom + bar_thickness + 2),
                                           health_color);
                    }
                }
                break;
            }
        }
    }

    void draw_text_with_shadow(ImDrawList* draw, const char* text, float x, float y,
                               float alpha, bool center_above = true) {
        ImVec2 text_size = ImGui::CalcTextSize(text);
        float text_x = center_above ? x - text_size.x * 0.5f : x - text_size.x * 0.5f;
        float text_y = center_above ? y - text_size.y : y;

        ImU32 shadow = IM_COL32(0, 0, 0, (int)(180 * alpha));
        ImU32 white = IM_COL32(255, 255, 255, (int)(255 * alpha));

        draw->AddText(ImVec2(text_x + 1, text_y + 1), shadow, text);
        draw->AddText(ImVec2(text_x, text_y), white, text);
    }

    void draw_offscreen_indicator(ImDrawList* draw, float screen_w, float screen_h,
                                  const Entity& ent, const Entity& local, const ViewMatrix& view) {
        (void)view;

        float dist = ent.distance_to(local.position);
        if (dist > offscreen.max_distance) return;

        float center_x = screen_w / 2.0f;
        float center_y = screen_h / 2.0f;

        // Calculate direction to enemy
        float dx = ent.position.x - local.position.x;
        float dy = ent.position.y - local.position.y;
        float angle = std::atan2(dy, dx);

        // Position on screen edge
        float radius = std::min(screen_w, screen_h) * offscreen.radius;
        float arrow_x = center_x + std::cos(angle) * radius;
        float arrow_y = center_y + std::sin(angle) * radius;

        ImU32 color = ImGui::ColorConvertFloat4ToU32(offscreen.color);

        // Draw arrow
        float arrow_angle = angle + 3.14159f;
        float arrow_len = offscreen.arrow_size;
        float arrow_width = arrow_len * 0.5f;

        ImVec2 tip(arrow_x, arrow_y);
        ImVec2 left(arrow_x + std::cos(arrow_angle - 0.5f) * arrow_len,
                    arrow_y + std::sin(arrow_angle - 0.5f) * arrow_len);
        ImVec2 right(arrow_x + std::cos(arrow_angle + 0.5f) * arrow_len,
                     arrow_y + std::sin(arrow_angle + 0.5f) * arrow_len);

        draw->AddTriangleFilled(tip, left, right, color);

        if (offscreen.show_distance) {
            char dist_text[16];
            snprintf(dist_text, sizeof(dist_text), "%.0fm", dist);
            ImVec2 text_size = ImGui::CalcTextSize(dist_text);
            draw->AddText(ImVec2(arrow_x - text_size.x / 2, arrow_y + arrow_len + 2),
                         IM_COL32(255, 255, 255, 255), dist_text);
        }
    }

    void render_menu() override {
        // Tab bar
        if (ImGui::BeginTabBar("ESP_Tabs")) {
            if (ImGui::BeginTabItem("General")) {
                current_tab = 0;
                render_general_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Enemies")) {
                current_tab = 1;
                render_target_settings("enemy", enemies);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Teammates")) {
                current_tab = 2;
                render_target_settings("team", teammates);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Off-Screen")) {
                current_tab = 3;
                render_offscreen_tab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void render_general_tab() {
        ImGui::Checkbox("Team Check (Enemies Only)", &team_check);

        ImGui::Separator();
        ImGui::Text("Animation");
        ImGui::Checkbox("Distance Fade", &animation.enabled);
        if (animation.enabled) {
            ImGui::SliderFloat("Fade Start", &animation.fade_start, 50.0f, 500.0f, "%.0f");
            ImGui::SliderFloat("Fade End", &animation.fade_end, 10.0f, 100.0f, "%.0f");
        }
    }

    void render_target_settings(const char* id, TargetSettings& s) {
        ImGui::PushID(id);

        ImGui::Checkbox("Enabled", &s.enabled);
        ImGui::SliderFloat("Max Distance", &s.max_distance, 0.0f, 2000.0f, "%.0f");

        ImGui::Separator();
        ImGui::Text("Box");

        const char* box_styles[] = { "None", "2D Box", "Corner" };
        int box_idx = (int)s.box_style;
        if (ImGui::Combo("Box Style", &box_idx, box_styles, 3)) {
            s.box_style = (BoxStyle)box_idx;
        }

        if (s.box_style != BoxStyle::None) {
            ImGui::SliderFloat("Box Thickness", &s.box_thickness, 1.0f, 5.0f, "%.1f");
            ImGui::ColorEdit4("Visible Color", &s.visible_color.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Not Visible Color", &s.invisible_color.x, ImGuiColorEditFlags_NoInputs);
        }

        ImGui::Separator();
        ImGui::Text("Skeleton");
        ImGui::Checkbox("Show Skeleton", &s.show_skeleton);
        if (s.show_skeleton) {
            ImGui::SliderFloat("Skeleton Thickness", &s.skeleton_thickness, 1.0f, 5.0f, "%.1f");
            ImGui::ColorEdit4("Skeleton Visible", &s.skeleton_visible.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Skeleton Invisible", &s.skeleton_invisible.x, ImGuiColorEditFlags_NoInputs);
        }

        ImGui::Separator();
        ImGui::Text("Health Bar");
        ImGui::Checkbox("Show Health", &s.show_health);
        if (s.show_health) {
            const char* health_styles[] = { "Left", "Top", "Bottom", "Seer" };
            int health_idx = (int)s.health_style;
            if (ImGui::Combo("Health Style", &health_idx, health_styles, 4)) {
                s.health_style = (HealthStyle)health_idx;
            }
        }

        ImGui::Separator();
        ImGui::Text("Info");
        ImGui::Checkbox("Show Name", &s.show_name);
        ImGui::Checkbox("Show Distance", &s.show_distance);
        ImGui::Checkbox("Show Weapon", &s.show_weapon);

        ImGui::PopID();
    }

    void render_offscreen_tab() {
        ImGui::Checkbox("Enable Off-Screen Indicators", &offscreen.enabled);
        if (offscreen.enabled) {
            ImGui::Checkbox("Enemy Only", &offscreen.enemy_only);
            ImGui::Checkbox("Show Distance", &offscreen.show_distance);
            ImGui::SliderFloat("Max Distance", &offscreen.max_distance, 50.0f, 500.0f, "%.0f");
            ImGui::SliderFloat("Radius", &offscreen.radius, 0.2f, 0.45f, "%.2f");
            ImGui::SliderFloat("Arrow Size", &offscreen.arrow_size, 5.0f, 20.0f, "%.0f");
            ImGui::ColorEdit4("Arrow Color", &offscreen.color.x, ImGuiColorEditFlags_NoInputs);
        }
    }
};

std::unique_ptr<Feature> create_esp_feature() {
    return std::make_unique<EspFeature>();
}
