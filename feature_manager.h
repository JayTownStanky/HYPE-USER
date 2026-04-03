#ifndef FEATURE_MANAGER_H
#define FEATURE_MANAGER_H

#include "feature.h"
#include "config.h"
#include <vector>
#include <memory>
#include <unordered_map>

class FeatureManager {
public:
    FeatureManager() = default;
    ~FeatureManager() = default;

    // Register a feature
    void register_feature(std::unique_ptr<Feature> feature);

    // Load settings from config
    void load_config(const Config& cfg);

    // Save settings to config
    void save_config(Config& cfg);

    // Tick all enabled features
    void tick(GameState& state, HvMemory& mem);

    // Render all enabled features
    void render(ImDrawList* draw, float screen_w, float screen_h, const GameState& state);

    // Render menu for all features
    void render_menu();

    // Check hotkeys for toggling features
    void check_hotkeys();

    // Get feature by name
    Feature* get_feature(const std::string& name);

    // Get all features
    const std::vector<std::unique_ptr<Feature>>& features() const { return m_features; }

private:
    std::vector<std::unique_ptr<Feature>> m_features;
    std::unordered_map<int, bool> m_key_states;  // For edge detection
};

#endif // FEATURE_MANAGER_H
