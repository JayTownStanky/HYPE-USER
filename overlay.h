#ifndef OVERLAY_H
#define OVERLAY_H

#include <string>
#include <d3d11.h>
#include <Windows.h>

// Forward declare ImDrawList to avoid including imgui.h in header
struct ImDrawList;

class Overlay {
public:
    Overlay() = default;
    ~Overlay();

    // Initialize overlay window and DX11
    bool init(int width, int height, const std::string& target_window_title);

    // Begin frame - poll messages, start ImGui frame
    // Returns false if should skip frame (target minimized/closed)
    bool begin_frame();

    // End frame - render ImGui, present
    void end_frame();

    // Check if overlay is still open
    bool is_open() const { return m_open; }

    // Shutdown and cleanup
    void shutdown();

    // Get overlay window handle
    HWND hwnd() const { return m_hwnd; }

    // Get target window handle
    HWND target_hwnd() const { return m_target_hwnd; }

    // Get D3D11 device
    ID3D11Device* device() const { return m_device; }

    // Get ImGui background draw list for ESP rendering
    ImDrawList* draw_list();

    // Toggle between click-through and input capture
    void toggle_input();

    // Check if input is enabled
    bool input_enabled() const { return m_input_enabled; }

    // Get overlay dimensions
    int width() const { return m_width; }
    int height() const { return m_height; }

    // Set vsync
    void set_vsync(bool enabled) { m_vsync = enabled; }

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    bool create_window(int width, int height);
    bool create_device();
    bool create_render_target();
    void cleanup_render_target();

    HWND m_hwnd = nullptr;
    HWND m_target_hwnd = nullptr;
    std::string m_target_title;
    std::string m_class_name;

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    IDXGISwapChain* m_swapchain = nullptr;
    ID3D11RenderTargetView* m_rtv = nullptr;

    int m_width = 0;
    int m_height = 0;
    bool m_open = false;
    bool m_input_enabled = false;
    bool m_vsync = false;
    bool m_imgui_initialized = false;
};

#endif // OVERLAY_H
