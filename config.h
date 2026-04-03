#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include <cstdint>

// Feature configuration
struct FeatureConfig {
    bool enabled = false;
    int hotkey = 0;

    // Feature-specific fields
    float fov = 0.0f;
    float smooth = 0.0f;
    int bone_id = 0;
    bool team_check = true;
    bool visibility_check = false;
    float max_distance = 0.0f;
    int color_r = 255;
    int color_g = 255;
    int color_b = 255;
    int color_a = 255;
    float thickness = 1.0f;
};

// Game configuration
struct GameConfig {
    std::string process_name;
    std::string offsets_version;
    std::unordered_map<std::string, uint64_t> offsets;
};

// Overlay configuration
struct OverlayConfig {
    int target_fps = 144;
    bool vsync = false;
    int width = 0;   // 0 = auto (screen width)
    int height = 0;  // 0 = auto (screen height)
};

// Root configuration
struct Config {
    GameConfig game;
    std::unordered_map<std::string, FeatureConfig> features;
    OverlayConfig overlay;
};

// Load configuration from JSON file
bool load_config(const std::string& path, Config& out);

// Save configuration to JSON file
bool save_config(const std::string& path, const Config& cfg);

// Get offset by name, returns 0 if not found
uint64_t get_offset(const Config& cfg, const std::string& name);

#endif // CONFIG_H
