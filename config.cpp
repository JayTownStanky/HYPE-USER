#include "config.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>

using json = nlohmann::json;

static uint64_t parse_hex(const std::string& str) {
    try {
        if (str.empty()) return 0;
        return std::stoull(str, nullptr, 16);
    } catch (...) {
        return 0;
    }
}

bool load_config(const std::string& path, Config& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to open config file: {}", path);
        return false;
    }

    try {
        json j = json::parse(file);

        // Parse game config
        if (j.contains("game")) {
            auto& g = j["game"];
            out.game.process_name = g.value("process_name", "");
            out.game.offsets_version = g.value("offsets_version", "");

            if (g.contains("offsets") && g["offsets"].is_object()) {
                for (auto& [key, val] : g["offsets"].items()) {
                    if (val.is_string()) {
                        out.game.offsets[key] = parse_hex(val.get<std::string>());
                    } else if (val.is_number_unsigned()) {
                        out.game.offsets[key] = val.get<uint64_t>();
                    }
                }
            }
        }

        // Parse features
        if (j.contains("features") && j["features"].is_object()) {
            for (auto& [name, feat] : j["features"].items()) {
                FeatureConfig fc;
                fc.enabled = feat.value("enabled", false);
                fc.hotkey = feat.value("hotkey", 0);
                fc.fov = feat.value("fov", 0.0f);
                fc.smooth = feat.value("smooth", 0.0f);
                fc.bone_id = feat.value("bone_id", 0);
                fc.team_check = feat.value("team_check", true);
                fc.visibility_check = feat.value("visibility_check", false);
                fc.max_distance = feat.value("max_distance", 0.0f);
                fc.color_r = feat.value("color_r", 255);
                fc.color_g = feat.value("color_g", 255);
                fc.color_b = feat.value("color_b", 255);
                fc.color_a = feat.value("color_a", 255);
                fc.thickness = feat.value("thickness", 1.0f);
                out.features[name] = fc;
            }
        }

        // Parse overlay config
        if (j.contains("overlay")) {
            auto& o = j["overlay"];
            out.overlay.target_fps = o.value("target_fps", 144);
            out.overlay.vsync = o.value("vsync", false);
            out.overlay.width = o.value("width", 0);
            out.overlay.height = o.value("height", 0);
        }

        spdlog::info("Loaded config: {} offsets, {} features",
                     out.game.offsets.size(), out.features.size());
        return true;

    } catch (const json::exception& e) {
        spdlog::error("JSON parse error: {}", e.what());
        return false;
    }
}

bool save_config(const std::string& path, const Config& cfg) {
    try {
        json j;

        // Game config
        j["game"]["process_name"] = cfg.game.process_name;
        j["game"]["offsets_version"] = cfg.game.offsets_version;
        for (auto& [key, val] : cfg.game.offsets) {
            char hex[32];
            snprintf(hex, sizeof(hex), "0x%llX", static_cast<unsigned long long>(val));
            j["game"]["offsets"][key] = hex;
        }

        // Features
        for (auto& [name, fc] : cfg.features) {
            json f;
            f["enabled"] = fc.enabled;
            f["hotkey"] = fc.hotkey;
            f["fov"] = fc.fov;
            f["smooth"] = fc.smooth;
            f["bone_id"] = fc.bone_id;
            f["team_check"] = fc.team_check;
            f["visibility_check"] = fc.visibility_check;
            f["max_distance"] = fc.max_distance;
            f["color_r"] = fc.color_r;
            f["color_g"] = fc.color_g;
            f["color_b"] = fc.color_b;
            f["color_a"] = fc.color_a;
            f["thickness"] = fc.thickness;
            j["features"][name] = f;
        }

        // Overlay
        j["overlay"]["target_fps"] = cfg.overlay.target_fps;
        j["overlay"]["vsync"] = cfg.overlay.vsync;
        j["overlay"]["width"] = cfg.overlay.width;
        j["overlay"]["height"] = cfg.overlay.height;

        std::ofstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open config file for writing: {}", path);
            return false;
        }

        file << j.dump(4);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save config: {}", e.what());
        return false;
    }
}

uint64_t get_offset(const Config& cfg, const std::string& name) {
    auto it = cfg.game.offsets.find(name);
    if (it != cfg.game.offsets.end()) {
        return it->second;
    }
    spdlog::warn("Offset not found: {}", name);
    return 0;
}
