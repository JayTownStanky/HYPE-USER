#include "../feature.h"
#include "../offsets.hpp"
#include <imgui.h>
#include <Windows.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>
#include <memory>

class AimbotFeature : public Feature {
public:
    AimbotFeature() {
        m_name = "Aimbot";
        m_hotkey = VK_XBUTTON2;
    }

    // Weapon classes for per-weapon profiles
    enum class WeaponClass {
        Default = 0, AR, SMG, LMG, Sniper, Shotgun, Pistol, Wingman, COUNT
    };

    struct AimConfig {
        float fov = 15.0f;
        float smooth = 10.0f;
        float recoil_x = 1.0f;
        float recoil_y = 1.0f;
    };

    struct WeaponProfile {
        bool enabled = false;
        float fov_override = 0.0f;      // 0 = use base
        float smooth_override = 0.0f;   // 0 = use base
    };

    // Base configs
    AimConfig no_scope{ 15.0f, 10.0f, 1.0f, 1.0f };
    AimConfig scoped{ 12.0f, 9.0f, 1.2f, 1.2f };

    // Weapon profiles
    struct {
        bool use_profiles = false;
        WeaponProfile profiles[8] = {
            { false, 0, 0 },       // Default
            { true, 15.0f, 10.0f }, // AR - balanced
            { true, 18.0f, 8.0f },  // SMG - faster, wider
            { true, 14.0f, 12.0f }, // LMG - slower
            { true, 8.0f, 14.0f },  // Sniper - tight, slow
            { true, 22.0f, 6.0f },  // Shotgun - wide, fast
            { true, 16.0f, 9.0f },  // Pistol - medium
            { true, 10.0f, 12.0f }, // Wingman - tight, slow
        };
    } weapon_profiles;

    // Target settings
    struct {
        int priority_mode = 1;  // 0=distance, 1=crosshair, 2=health
        bool target_enemies = true;
        bool target_teammates = false;
        bool visible_only = true;
        bool ignore_knocked = true;
        float max_distance = 300.0f;
        float min_distance = 1.0f;
    } target_settings;

    // Humanization
    struct {
        bool enabled = true;
        int mode = 2;  // 0=none, 1=linear, 2=bezier, 3=micro-adjust
        float micro_adjust_scale = 0.05f;
        float curve_strength = 0.15f;
        float time_variation_min = 0.8f;
        float time_variation_max = 1.2f;
        float bezier_speed = 0.8f;

        // State
        float bezier_progress = 0.0f;
        Vec2 bezier_start;
        Vec2 bezier_control;
        Vec2 bezier_end;
        bool bezier_active = false;
    } humanize;

    // Visual settings
    struct {
        bool show_fov = true;
        float fov_thickness = 1.5f;
        ImVec4 normal_color = ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
        ImVec4 target_color = ImVec4(1.0f, 0.2f, 0.2f, 0.8f);
        bool show_target_indicator = true;
        float indicator_size = 8.0f;
    } visuals;

    // Bone targeting
    int aim_bone = 0;  // 0=head, 1=neck, 2=chest, 3=stomach
    bool auto_bone = true;

    // State
    bool is_aiming = false;
    uint64_t current_target = 0;
    std::chrono::steady_clock::time_point last_target_switch;

    // RNG for humanization
    std::mt19937 rng{ std::random_device{}() };

    void tick(GameState& state, HvMemory& mem) override {
        (void)mem;
        is_aiming = false;

        if (!state.ready()) return;

        // Only active while hotkey is held
        if (!(GetAsyncKeyState(m_hotkey) & 0x8000)) {
            reset_humanize();
            current_target = 0;
            return;
        }

        const auto& local = state.local_player();
        if (!local.valid() || !local.alive()) return;

        // Find best target
        const Entity* best = find_best_target(state, local);
        if (!best) {
            reset_humanize();
            current_target = 0;
            return;
        }

        is_aiming = true;

        // Handle target switching
        if (best->address != current_target) {
            current_target = best->address;
            last_target_switch = std::chrono::steady_clock::now();
            init_bezier_path(state, *best);
        }

        // Get current config
        AimConfig config = get_active_config();

        // Calculate target screen position
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        float center_x = screen_w / 2.0f;
        float center_y = screen_h / 2.0f;

        Vec3 target_pos = get_target_bone_position(*best);
        Vec2 target_screen;
        if (!state.view_matrix().world_to_screen(target_pos, target_screen, screen_w, screen_h)) {
            return;
        }

        // Calculate delta
        float dx = target_screen.x - center_x;
        float dy = target_screen.y - center_y;

        // Apply humanization
        if (humanize.enabled) {
            apply_humanization(dx, dy, config.smooth);
        } else {
            // Simple smoothing
            dx /= config.smooth;
            dy /= config.smooth;
        }

        // Clamp movement
        dx = std::clamp(dx, -50.0f, 50.0f);
        dy = std::clamp(dy, -50.0f, 50.0f);

        // Apply mouse movement
        if (std::abs(dx) > 0.1f || std::abs(dy) > 0.1f) {
            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            input.mi.dx = (LONG)dx;
            input.mi.dy = (LONG)dy;
            SendInput(1, &input, sizeof(INPUT));
        }
    }

    const Entity* find_best_target(const GameState& state, const Entity& local) {
        const Entity* best = nullptr;
        float best_score = FLT_MAX;

        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        float center_x = screen_w / 2.0f;
        float center_y = screen_h / 2.0f;

        AimConfig config = get_active_config();

        for (const auto& ent : state.entities()) {
            if (!ent.alive()) continue;
            if (ent.address == local.address) continue;

            bool is_enemy = ent.team != local.team;
            if (target_settings.target_enemies && !is_enemy) continue;
            if (!target_settings.target_enemies && is_enemy) continue;

            float dist = ent.distance_to(local.position);
            if (dist < target_settings.min_distance) continue;
            if (target_settings.max_distance > 0 && dist > target_settings.max_distance) continue;

            // Check if in FOV
            Vec3 target_pos = get_target_bone_position(ent);
            Vec2 screen;
            if (!state.view_matrix().world_to_screen(target_pos, screen, screen_w, screen_h)) continue;

            float dx = screen.x - center_x;
            float dy = screen.y - center_y;
            float crosshair_dist = std::sqrt(dx * dx + dy * dy);

            // Convert to FOV degrees
            float fov_dist = crosshair_dist / (screen_w / 90.0f);
            if (fov_dist > config.fov) continue;

            // Calculate score based on priority mode
            float score = 0.0f;
            switch (target_settings.priority_mode) {
                case 0: score = dist; break;                    // Distance
                case 1: score = crosshair_dist; break;          // Crosshair
                case 2: score = (float)ent.health; break;       // Health
            }

            if (score < best_score) {
                best_score = score;
                best = &ent;
            }
        }

        return best;
    }

    Vec3 get_target_bone_position(const Entity& ent) {
        // Use head by default, fall back to position + offset
        if (!ent.head_position.is_zero()) {
            return ent.head_position;
        }

        Vec3 pos = ent.position;
        switch (aim_bone) {
            case 0: pos.z += 64.0f; break;  // Head
            case 1: pos.z += 58.0f; break;  // Neck
            case 2: pos.z += 45.0f; break;  // Chest
            case 3: pos.z += 30.0f; break;  // Stomach
        }
        return pos;
    }

    AimConfig get_active_config() {
        // TODO: detect scope state and weapon class
        return no_scope;
    }

    void init_bezier_path(const GameState& state, const Entity& target) {
        if (humanize.mode != 2) return;

        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);

        Vec3 target_pos = get_target_bone_position(target);
        Vec2 target_screen;
        if (!state.view_matrix().world_to_screen(target_pos, target_screen, screen_w, screen_h)) {
            humanize.bezier_active = false;
            return;
        }

        // Random control point offset
        std::uniform_real_distribution<float> offset_dist(-50.0f, 50.0f);

        humanize.bezier_start = Vec2(screen_w / 2.0f, screen_h / 2.0f);
        humanize.bezier_end = target_screen;
        humanize.bezier_control = Vec2(
            (humanize.bezier_start.x + humanize.bezier_end.x) / 2.0f + offset_dist(rng),
            (humanize.bezier_start.y + humanize.bezier_end.y) / 2.0f + offset_dist(rng)
        );
        humanize.bezier_progress = 0.0f;
        humanize.bezier_active = true;
    }

    void apply_humanization(float& dx, float& dy, float smooth) {
        switch (humanize.mode) {
            case 1: // Linear with jitter
                apply_linear_humanize(dx, dy, smooth);
                break;
            case 2: // Bezier curve
                apply_bezier_humanize(dx, dy, smooth);
                break;
            case 3: // Micro adjustments
                apply_micro_adjust(dx, dy, smooth);
                break;
            default:
                dx /= smooth;
                dy /= smooth;
                break;
        }
    }

    void apply_linear_humanize(float& dx, float& dy, float smooth) {
        // Add time variation
        std::uniform_real_distribution<float> time_dist(humanize.time_variation_min, humanize.time_variation_max);
        float time_factor = time_dist(rng);

        dx /= (smooth * time_factor);
        dy /= (smooth * time_factor);

        // Add small random offset
        std::uniform_real_distribution<float> jitter(-0.5f, 0.5f);
        dx += jitter(rng);
        dy += jitter(rng);
    }

    void apply_bezier_humanize(float& dx, float& dy, float smooth) {
        if (!humanize.bezier_active) {
            dx /= smooth;
            dy /= smooth;
            return;
        }

        // Progress along bezier curve
        humanize.bezier_progress += humanize.bezier_speed / smooth;
        if (humanize.bezier_progress >= 1.0f) {
            humanize.bezier_progress = 1.0f;
            humanize.bezier_active = false;
        }

        float t = humanize.bezier_progress;

        // Quadratic bezier: B(t) = (1-t)²P0 + 2(1-t)tP1 + t²P2
        float one_minus_t = 1.0f - t;
        float bezier_x = one_minus_t * one_minus_t * humanize.bezier_start.x +
                        2.0f * one_minus_t * t * humanize.bezier_control.x +
                        t * t * humanize.bezier_end.x;
        float bezier_y = one_minus_t * one_minus_t * humanize.bezier_start.y +
                        2.0f * one_minus_t * t * humanize.bezier_control.y +
                        t * t * humanize.bezier_end.y;

        // Delta from current position to bezier point
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        float center_x = screen_w / 2.0f;
        float center_y = screen_h / 2.0f;

        dx = (bezier_x - center_x) / smooth;
        dy = (bezier_y - center_y) / smooth;
    }

    void apply_micro_adjust(float& dx, float& dy, float smooth) {
        dx /= smooth;
        dy /= smooth;

        // Add micro-adjustments
        std::uniform_real_distribution<float> micro(-humanize.micro_adjust_scale, humanize.micro_adjust_scale);
        dx *= (1.0f + micro(rng));
        dy *= (1.0f + micro(rng));
    }

    void reset_humanize() {
        humanize.bezier_active = false;
        humanize.bezier_progress = 0.0f;
    }

    void render(ImDrawList* draw, float screen_w, float screen_h, const GameState& state) override {
        (void)state;

        if (!visuals.show_fov) return;

        float center_x = screen_w / 2.0f;
        float center_y = screen_h / 2.0f;

        AimConfig config = get_active_config();
        float fov_radius = config.fov * (screen_w / 90.0f);

        ImVec4 color = is_aiming ? visuals.target_color : visuals.normal_color;
        ImU32 col32 = ImGui::ColorConvertFloat4ToU32(color);

        draw->AddCircle(ImVec2(center_x, center_y), fov_radius, col32, 64, visuals.fov_thickness);

        // Target indicator
        if (is_aiming && visuals.show_target_indicator) {
            float ind_size = visuals.indicator_size;
            ImU32 ind_col = IM_COL32(255, 50, 50, 255);
            draw->AddCircleFilled(ImVec2(center_x, center_y), ind_size / 2, ind_col);
        }
    }

    void render_menu() override {
        if (ImGui::BeginTabBar("Aimbot_Tabs")) {
            if (ImGui::BeginTabItem("General")) {
                render_general_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Targeting")) {
                render_targeting_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Humanization")) {
                render_humanize_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Weapons")) {
                render_weapons_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Visuals")) {
                render_visuals_tab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void render_general_tab() {
        ImGui::Text("Base Config (No Scope)");
        ImGui::SliderFloat("FOV##noscope", &no_scope.fov, 1.0f, 30.0f, "%.1f deg");
        ImGui::SliderFloat("Smooth##noscope", &no_scope.smooth, 1.0f, 20.0f, "%.1f");

        ImGui::Separator();
        ImGui::Text("Scoped Config");
        ImGui::SliderFloat("FOV##scoped", &scoped.fov, 1.0f, 20.0f, "%.1f deg");
        ImGui::SliderFloat("Smooth##scoped", &scoped.smooth, 1.0f, 20.0f, "%.1f");

        ImGui::Separator();
        ImGui::Text("Bone Target");
        const char* bones[] = { "Head", "Neck", "Chest", "Stomach" };
        ImGui::Combo("Aim Bone", &aim_bone, bones, 4);
        ImGui::Checkbox("Auto Bone Selection", &auto_bone);
    }

    void render_targeting_tab() {
        const char* priority_modes[] = { "Distance", "Crosshair", "Lowest Health" };
        ImGui::Combo("Priority Mode", &target_settings.priority_mode, priority_modes, 3);

        ImGui::Separator();
        ImGui::Checkbox("Target Enemies", &target_settings.target_enemies);
        ImGui::Checkbox("Target Teammates", &target_settings.target_teammates);
        ImGui::Checkbox("Visible Only", &target_settings.visible_only);
        ImGui::Checkbox("Ignore Knocked", &target_settings.ignore_knocked);

        ImGui::Separator();
        ImGui::SliderFloat("Max Distance", &target_settings.max_distance, 0.0f, 1000.0f, "%.0f");
        ImGui::SliderFloat("Min Distance", &target_settings.min_distance, 0.0f, 50.0f, "%.0f");
    }

    void render_humanize_tab() {
        ImGui::Checkbox("Enable Humanization", &humanize.enabled);
        if (!humanize.enabled) return;

        const char* modes[] = { "None", "Linear + Jitter", "Bezier Curve", "Micro Adjustments" };
        ImGui::Combo("Mode", &humanize.mode, modes, 4);

        ImGui::Separator();

        if (humanize.mode == 1) {
            ImGui::SliderFloat("Time Variation Min", &humanize.time_variation_min, 0.5f, 1.0f, "%.2f");
            ImGui::SliderFloat("Time Variation Max", &humanize.time_variation_max, 1.0f, 1.5f, "%.2f");
        }

        if (humanize.mode == 2) {
            ImGui::SliderFloat("Bezier Speed", &humanize.bezier_speed, 0.1f, 2.0f, "%.2f");
            ImGui::SliderFloat("Curve Strength", &humanize.curve_strength, 0.0f, 0.5f, "%.2f");
        }

        if (humanize.mode == 3) {
            ImGui::SliderFloat("Micro Adjust Scale", &humanize.micro_adjust_scale, 0.01f, 0.2f, "%.3f");
        }
    }

    void render_weapons_tab() {
        ImGui::Checkbox("Use Weapon Profiles", &weapon_profiles.use_profiles);
        if (!weapon_profiles.use_profiles) return;

        ImGui::Separator();

        const char* weapon_names[] = { "Default", "AR", "SMG", "LMG", "Sniper", "Shotgun", "Pistol", "Wingman" };

        for (int i = 1; i < 8; i++) {
            ImGui::PushID(i);
            if (ImGui::TreeNode(weapon_names[i])) {
                ImGui::Checkbox("Override Enabled", &weapon_profiles.profiles[i].enabled);
                if (weapon_profiles.profiles[i].enabled) {
                    ImGui::SliderFloat("FOV", &weapon_profiles.profiles[i].fov_override, 1.0f, 30.0f, "%.1f");
                    ImGui::SliderFloat("Smooth", &weapon_profiles.profiles[i].smooth_override, 1.0f, 20.0f, "%.1f");
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    void render_visuals_tab() {
        ImGui::Checkbox("Show FOV Circle", &visuals.show_fov);
        if (visuals.show_fov) {
            ImGui::SliderFloat("FOV Thickness", &visuals.fov_thickness, 1.0f, 5.0f, "%.1f");
            ImGui::ColorEdit4("Normal Color", &visuals.normal_color.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Target Color", &visuals.target_color.x, ImGuiColorEditFlags_NoInputs);
        }

        ImGui::Separator();
        ImGui::Checkbox("Show Target Indicator", &visuals.show_target_indicator);
        if (visuals.show_target_indicator) {
            ImGui::SliderFloat("Indicator Size", &visuals.indicator_size, 4.0f, 20.0f, "%.0f");
        }
    }
};

std::unique_ptr<Feature> create_aimbot_feature() {
    return std::make_unique<AimbotFeature>();
}
