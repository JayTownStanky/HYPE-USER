#include "overlay.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <spdlog/spdlog.h>
#include <intrin.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ImGui Win32 message handler (defined in imgui_impl_win32.cpp)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Overlay::~Overlay() {
    shutdown();
}

bool Overlay::init(int width, int height, const std::string& target_window_title) {
    m_target_title = target_window_title;
    m_width = width;
    m_height = height;

    // Find target window
    m_target_hwnd = FindWindowA(nullptr, target_window_title.c_str());
    if (!m_target_hwnd) {
        spdlog::error("Target window not found: {}", target_window_title);
        return false;
    }

    // Get target window rect if width/height are 0 (auto)
    if (m_width == 0 || m_height == 0) {
        RECT rect;
        if (GetWindowRect(m_target_hwnd, &rect)) {
            m_width = rect.right - rect.left;
            m_height = rect.bottom - rect.top;
        } else {
            m_width = GetSystemMetrics(SM_CXSCREEN);
            m_height = GetSystemMetrics(SM_CYSCREEN);
        }
    }

    if (!create_window(m_width, m_height)) {
        spdlog::error("Failed to create overlay window");
        return false;
    }

    if (!create_device()) {
        spdlog::error("Failed to create D3D11 device");
        return false;
    }

    if (!create_render_target()) {
        spdlog::error("Failed to create render target");
        return false;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // Disable imgui.ini

    // Dark theme with transparency
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha = 0.95f;
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    // Slightly transparent backgrounds
    style.Colors[ImGuiCol_WindowBg].w = 0.90f;
    style.Colors[ImGuiCol_PopupBg].w = 0.90f;

    // Initialize backends
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_device, m_context);
    m_imgui_initialized = true;

    m_open = true;
    spdlog::info("Overlay initialized: {}x{}", m_width, m_height);
    return true;
}

bool Overlay::create_window(int width, int height) {
    // Generate randomized class name
    uint64_t seed = __rdtsc() ^ GetTickCount64();
    char class_name[32];
    snprintf(class_name, sizeof(class_name), "OVL_%llX", seed);
    m_class_name = class_name;

    // Register window class
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = m_class_name.c_str();

    if (!RegisterClassExA(&wc)) {
        return false;
    }

    // Get target window position
    RECT target_rect;
    GetWindowRect(m_target_hwnd, &target_rect);

    // Create transparent overlay window
    DWORD ex_style = WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED;
    DWORD style = WS_POPUP;

    m_hwnd = CreateWindowExA(
        ex_style,
        m_class_name.c_str(),
        "",
        style,
        target_rect.left,
        target_rect.top,
        width,
        height,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    if (!m_hwnd) {
        return false;
    }

    // Set color key transparency (magenta = transparent)
    SetLayeredWindowAttributes(m_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    // Show window
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hwnd);

    return true;
}

bool Overlay::create_device() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = m_width;
    sd.BufferDesc.Height = m_height;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 0;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT create_flags = 0;
#ifdef _DEBUG
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_level;
    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        create_flags,
        feature_levels,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &m_swapchain,
        &m_device,
        &feature_level,
        &m_context
    );

    return SUCCEEDED(hr);
}

bool Overlay::create_render_target() {
    ID3D11Texture2D* back_buffer = nullptr;
    HRESULT hr = m_swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateRenderTargetView(back_buffer, nullptr, &m_rtv);
    back_buffer->Release();

    return SUCCEEDED(hr);
}

void Overlay::cleanup_render_target() {
    if (m_rtv) {
        m_rtv->Release();
        m_rtv = nullptr;
    }
}

bool Overlay::begin_frame() {
    // Process messages
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_open = false;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (!m_open) {
        return false;
    }

    // Check if target window still exists
    if (!IsWindow(m_target_hwnd)) {
        m_open = false;
        return false;
    }

    // Check if target is minimized
    if (IsIconic(m_target_hwnd)) {
        return false;
    }

    // Track target window position/size
    RECT target_rect;
    if (GetWindowRect(m_target_hwnd, &target_rect)) {
        int new_width = target_rect.right - target_rect.left;
        int new_height = target_rect.bottom - target_rect.top;

        // Move overlay to match target
        SetWindowPos(
            m_hwnd,
            HWND_TOPMOST,
            target_rect.left,
            target_rect.top,
            new_width,
            new_height,
            SWP_NOACTIVATE
        );

        // Handle resize
        if (new_width != m_width || new_height != m_height) {
            m_width = new_width;
            m_height = new_height;

            cleanup_render_target();
            HRESULT hr = m_swapchain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(hr)) {
                if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                    spdlog::error("Device lost during resize: 0x{:X}", static_cast<uint32_t>(hr));
                    m_open = false;
                } else {
                    spdlog::error("ResizeBuffers failed: 0x{:X}", static_cast<uint32_t>(hr));
                }
                return false;
            }

            if (!create_render_target()) {
                spdlog::error("Failed to create render target after resize");
                m_open = false;
                return false;
            }
        }
    }

    // Start ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    return true;
}

void Overlay::end_frame() {
    // Render ImGui
    ImGui::Render();

    // Clear to transparent black
    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
    m_context->ClearRenderTargetView(m_rtv, clear_color);

    // Render ImGui draw data
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present
    HRESULT hr = m_swapchain->Present(m_vsync ? 1 : 0, 0);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            spdlog::error("Device lost on Present: 0x{:X}", static_cast<uint32_t>(hr));
            m_open = false;
        } else {
            spdlog::warn("Present failed: 0x{:X}", static_cast<uint32_t>(hr));
        }
    }
}

ImDrawList* Overlay::draw_list() {
    return ImGui::GetBackgroundDrawList();
}

void Overlay::toggle_input() {
    m_input_enabled = !m_input_enabled;

    LONG_PTR ex_style = GetWindowLongPtrA(m_hwnd, GWL_EXSTYLE);

    if (m_input_enabled) {
        // Remove transparent flag to capture input
        ex_style &= ~WS_EX_TRANSPARENT;
    } else {
        // Add transparent flag for click-through
        ex_style |= WS_EX_TRANSPARENT;
    }

    SetWindowLongPtrA(m_hwnd, GWL_EXSTYLE, ex_style);

    // Force window to update
    SetWindowPos(
        m_hwnd,
        HWND_TOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED
    );
}

void Overlay::shutdown() {
    if (m_imgui_initialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_imgui_initialized = false;
    }

    cleanup_render_target();

    if (m_swapchain) {
        m_swapchain->Release();
        m_swapchain = nullptr;
    }

    if (m_context) {
        m_context->Release();
        m_context = nullptr;
    }

    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    if (!m_class_name.empty()) {
        UnregisterClassA(m_class_name.c_str(), GetModuleHandleA(nullptr));
        m_class_name.clear();
    }

    m_open = false;
}

LRESULT CALLBACK Overlay::wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        // Handle resize if needed
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}
