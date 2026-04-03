#include "feature_manager.h"
#include <Windows.h>
#include <memory>
#include <spdlog/spdlog.h>

void FeatureManager::register_feature(std::unique_ptr<Feature> feature) {
    spdlog::info("Registered feature: {}", feature->m_name);
    m_features.push_back(std::move(feature));
}

void FeatureManager::load_config(const Config& cfg) {
    for (auto& feature : m_features) {
        auto it = cfg.features.find(feature->m_name);
        if (it != cfg.features.end()) {
            feature->m_enabled = it->second.enabled;
            feature->m_hotkey = it->second.hotkey;
            spdlog::debug("Loaded config for {}: enabled={}, hotkey={}",
                         feature->m_name, feature->m_enabled, feature->m_hotkey);
        }
    }
}

void FeatureManager::save_config(Config& cfg) {
    for (auto& feature : m_features) {
        FeatureConfig fc;
        fc.enabled = feature->m_enabled;
        fc.hotkey = feature->m_hotkey;
        cfg.features[feature->m_name] = fc;
    }
}

void FeatureManager::tick(GameState& state, HvMemory& mem) {
    for (auto& feature : m_features) {
        if (feature->m_enabled) {
            feature->tick(state, mem);
        }
    }
}

void FeatureManager::render(ImDrawList* draw, float screen_w, float screen_h, const GameState& state) {
    for (auto& feature : m_features) {
        if (feature->m_enabled) {
            feature->render(draw, screen_w, screen_h, state);
        }
    }
}

void FeatureManager::render_menu() {
    // Style the menu window
    ImGui::SetNextWindowSize(ImVec2(450, 500), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("HV-Client", nullptr, flags);

    // Header with status
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "HV-Client v1.0");
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Connected");

    ImGui::Separator();

    // Navigation buttons
    static int selected_feature = -1;

    ImGui::BeginChild("NavPanel", ImVec2(120, 0), true);
    for (size_t i = 0; i < m_features.size(); i++) {
        auto& feature = m_features[i];

        // Color based on enabled state
        if (feature->m_enabled) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        }

        if (ImGui::Button(feature->m_name, ImVec2(105, 30))) {
            selected_feature = (int)i;
        }

        ImGui::PopStyleColor(2);
    }

    ImGui::Separator();

    // Settings button
    if (ImGui::Button("Settings", ImVec2(105, 30))) {
        selected_feature = -2;
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Content panel
    ImGui::BeginChild("ContentPanel", ImVec2(0, 0), true);

    if (selected_feature >= 0 && selected_feature < (int)m_features.size()) {
        auto& feature = m_features[selected_feature];

        // Feature header
        ImGui::Text("%s", feature->m_name);
        ImGui::SameLine(ImGui::GetWindowWidth() - 100);

        // Enable toggle
        bool enabled = feature->m_enabled;
        if (ImGui::Checkbox("Enabled", &enabled)) {
            feature->m_enabled = enabled;
        }

        // Hotkey display
        if (feature->m_hotkey != 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("[%d]", feature->m_hotkey);
        }

        ImGui::Separator();

        // Feature-specific menu
        feature->render_menu();

    } else if (selected_feature == -2) {
        // Settings panel
        ImGui::Text("Settings");
        ImGui::Separator();

        ImGui::Text("Hotkeys");
        ImGui::TextDisabled("INSERT - Toggle Menu");
        ImGui::TextDisabled("END - Exit");

        ImGui::Separator();

        ImGui::Text("Feature Hotkeys");
        for (auto& feature : m_features) {
            ImGui::PushID(feature->m_name);
            ImGui::Text("%s:", feature->m_name);
            ImGui::SameLine(150);

            char key_buf[32];
            if (feature->m_hotkey == 0) {
                snprintf(key_buf, sizeof(key_buf), "None");
            } else {
                snprintf(key_buf, sizeof(key_buf), "Key %d", feature->m_hotkey);
            }

            if (ImGui::Button(key_buf, ImVec2(80, 0))) {
                // TODO: Key binding capture
            }
            ImGui::PopID();
        }

    } else {
        // No selection - show overview
        ImGui::Text("Feature Overview");
        ImGui::Separator();

        for (auto& feature : m_features) {
            ImVec4 color = feature->m_enabled ?
                ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

            ImGui::TextColored(color, "[%s] %s",
                feature->m_enabled ? "ON" : "OFF", feature->m_name);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Click a feature to configure");
    }

    ImGui::EndChild();

    ImGui::End();
}

void FeatureManager::check_hotkeys() {
    for (auto& feature : m_features) {
        if (feature->m_hotkey == 0) continue;

        bool key_down = (GetAsyncKeyState(feature->m_hotkey) & 0x8000) != 0;
        bool was_down = m_key_states[feature->m_hotkey];

        // Toggle on key-down edge
        if (key_down && !was_down) {
            feature->m_enabled = !feature->m_enabled;
            spdlog::info("{} {}", feature->m_name, feature->m_enabled ? "enabled" : "disabled");
        }

        m_key_states[feature->m_hotkey] = key_down;
    }
}

Feature* FeatureManager::get_feature(const std::string& name) {
    for (auto& feature : m_features) {
        if (feature->m_name == name) {
            return feature.get();
        }
    }
    return nullptr;
}
