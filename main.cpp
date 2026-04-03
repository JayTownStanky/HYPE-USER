#include "hv_comms.h"
#include "mem.h"
#include "config.h"
#include "game_sdk.h"
#include "overlay.h"
#include "feature_manager.h"
#include "features/features.h"

#include <spdlog/spdlog.h>
#include <Windows.h>
#include <string>

constexpr const char* VERSION = "1.0.0";

int main(int argc, char* argv[]) {
    // Set up logging
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("========================================");
    spdlog::info("  HV-Client v{}", VERSION);
    spdlog::info("  Hypervisor-based memory access");
    spdlog::info("========================================");

    // Parse command line for config path
    std::string config_path = "config.json";
    if (argc > 1) {
        config_path = argv[1];
    }
    spdlog::info("Config: {}", config_path);

    // Load configuration
    Config cfg;
    if (!load_config(config_path, cfg)) {
        spdlog::error("Failed to load config file: {}", config_path);
        spdlog::info("Creating default config...");

        // Set some defaults
        cfg.game.process_name = "game.exe";
        cfg.overlay.target_fps = 144;
        cfg.overlay.vsync = false;
        save_config(config_path, cfg);
    }

    // Initialize hypervisor communication
    spdlog::info("Initializing hypervisor communication...");
    if (!hv::init()) {
        spdlog::error("Hypervisor not detected");
        spdlog::error("Ensure the hypervisor is running and VMMCALL/CPUID backdoor is active");
        MessageBoxA(nullptr, "Hypervisor not detected.\n\nEnsure the hypervisor is loaded.",
                    "HV-Client Error", MB_ICONERROR);
        return 1;
    }

    // Log detected channel
    const char* channel_name = "Unknown";
    switch (hv::active_channel()) {
        case hv::Channel::VMMCALL: channel_name = "VMMCALL"; break;
        case hv::Channel::CPUID:   channel_name = "CPUID"; break;
        default: break;
    }
    spdlog::info("Hypervisor detected via {} channel", channel_name);

    // Create memory interface
    HvMemory mem;

    // Initialize game state
    spdlog::info("Attaching to process: {}", cfg.game.process_name);
    GameState game_state;
    if (!game_state.init(mem, cfg)) {
        spdlog::error("Failed to attach to target process");
        spdlog::error("Ensure {} is running", cfg.game.process_name);
        MessageBoxA(nullptr, "Failed to attach to target process.\n\nEnsure the game is running.",
                    "HV-Client Error", MB_ICONERROR);
        return 1;
    }

    spdlog::info("Attached successfully:");
    spdlog::info("  CR3: 0x{:X}", mem.cr3());
    spdlog::info("  client.dll: 0x{:X}", game_state.client_base());
    if (game_state.engine_base() != 0) {
        spdlog::info("  engine.dll: 0x{:X}", game_state.engine_base());
    }

    // Create overlay
    spdlog::info("Creating overlay...");
    Overlay overlay;
    if (!overlay.init(cfg.overlay.width, cfg.overlay.height, cfg.game.process_name)) {
        spdlog::error("Failed to create overlay window");
        MessageBoxA(nullptr, "Failed to create overlay.\n\nEnsure the game window is visible.",
                    "HV-Client Error", MB_ICONERROR);
        return 1;
    }
    overlay.set_vsync(cfg.overlay.vsync);
    spdlog::info("Overlay created: {}x{}", overlay.width(), overlay.height());

    // Create and register features
    spdlog::info("Initializing features...");
    FeatureManager feature_manager;
    feature_manager.register_feature(create_esp_feature());
    feature_manager.register_feature(create_aimbot_feature());
    feature_manager.register_feature(create_radar_feature());
    feature_manager.register_feature(create_triggerbot_feature());
    feature_manager.register_feature(create_misc_feature());
    feature_manager.load_config(cfg);

    // Input state for menu toggle
    bool insert_was_pressed = false;

    spdlog::info("Entering main loop... Press INSERT to toggle menu");

    // Main loop
    while (overlay.is_open()) {
        // Check feature hotkeys
        feature_manager.check_hotkeys();

        // Check INSERT key for menu toggle (edge detect)
        bool insert_pressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insert_pressed && !insert_was_pressed) {
            overlay.toggle_input();
            spdlog::debug("Menu {}", overlay.input_enabled() ? "opened" : "closed");
        }
        insert_was_pressed = insert_pressed;

        // Check END key for exit
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            spdlog::info("Exit hotkey pressed");
            break;
        }

        // Update game state
        if (!game_state.update()) {
            // Skip frame if update fails (target minimized, etc.)
            Sleep(100);
            continue;
        }

        // Tick features
        feature_manager.tick(game_state, mem);

        // Begin render frame
        if (!overlay.begin_frame()) {
            Sleep(10);
            continue;
        }

        // Render menu if input enabled
        if (overlay.input_enabled()) {
            feature_manager.render_menu();
        }

        // Render features
        feature_manager.render(
            overlay.draw_list(),
            (float)overlay.width(),
            (float)overlay.height(),
            game_state
        );

        // End render frame
        overlay.end_frame();

        // Cap CPU usage if not using vsync
        if (!cfg.overlay.vsync) {
            Sleep(1);
        }
    }

    // Cleanup
    spdlog::info("Shutting down...");
    overlay.shutdown();

    // Save config on exit
    feature_manager.save_config(cfg);
    save_config(config_path, cfg);

    spdlog::info("Clean shutdown complete");
    return 0;
}
