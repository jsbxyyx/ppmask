#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")

class WeChatMask {
private:
    HWND m_wechatHwnd; // 微信窗口句柄
    HWND m_maskHwnd; // 遮罩窗口句柄
    HWND m_trayHwnd; // 托盘消息窗口句柄
    HINSTANCE m_hInstance; // 程序实例
    DWORD m_wechatProcessId; // 微信进程ID
    NOTIFYICONDATA m_trayData; // 托盘图标数据

    // 遮罩参数（从INI文件加载）
    int MASK_WIDTH;
    int MASK_HEIGHT;
    int MASK_OPACITY;
    int OFFSET_X;
    int OFFSET_Y;

    // 用于跟踪窗口位置
    RECT m_lastWechatRect;

    // INI文件路径
    const wchar_t *CONFIG_FILE = L"settings.ini";
    const wchar_t *CONFIG_SECTION = L"Mask";

    // 托盘相关常量
    enum {
        WM_TRAYICON = WM_USER + 1,
        ID_TRAYICON = 1,
        IDM_EXIT = 1001
    };

public:
    WeChatMask() : m_wechatHwnd(nullptr), m_maskHwnd(nullptr), m_trayHwnd(nullptr),
                   m_hInstance(GetModuleHandle(nullptr)), m_wechatProcessId(0) {
        ZeroMemory(&m_lastWechatRect, sizeof(RECT));
        ZeroMemory(&m_trayData, sizeof(NOTIFYICONDATA));
        m_trayData.cbSize = sizeof(NOTIFYICONDATA);

        // 从INI文件加载配置
        LoadConfigFromIni();
    }

    ~WeChatMask() {
        if (m_maskHwnd) {
            DestroyWindow(m_maskHwnd);
        }
        // 移除托盘图标
        if (m_trayHwnd) {
            Shell_NotifyIcon(NIM_DELETE, &m_trayData);
            DestroyWindow(m_trayHwnd);
        }
    }

    // 从INI文件加载配置
    void LoadConfigFromIni() {
        // 默认值
        MASK_WIDTH = 200;
        MASK_HEIGHT = 575;
        MASK_OPACITY = 245;
        OFFSET_X = 130;
        OFFSET_Y = 80;

        // 从INI文件读取配置
        MASK_WIDTH = GetPrivateProfileIntW(CONFIG_SECTION, L"Width", MASK_WIDTH, CONFIG_FILE);
        MASK_HEIGHT = GetPrivateProfileIntW(CONFIG_SECTION, L"Height", MASK_HEIGHT, CONFIG_FILE);
        MASK_OPACITY = GetPrivateProfileIntW(CONFIG_SECTION, L"Opacity", MASK_OPACITY, CONFIG_FILE);
        OFFSET_X = GetPrivateProfileIntW(CONFIG_SECTION, L"OffsetX", OFFSET_X, CONFIG_FILE);
        OFFSET_Y = GetPrivateProfileIntW(CONFIG_SECTION, L"OffsetY", OFFSET_Y, CONFIG_FILE);

        // 确保不透明度在有效范围内
        if (MASK_OPACITY < 0) MASK_OPACITY = 0;
        if (MASK_OPACITY > 255) MASK_OPACITY = 255;

        std::wcout << L"从配置文件加载的参数:" << std::endl;
        std::wcout << L"- 宽度: " << MASK_WIDTH << std::endl;
        std::wcout << L"- 高度: " << MASK_HEIGHT << std::endl;
        std::wcout << L"- 不透明度: " << MASK_OPACITY << std::endl;
        std::wcout << L"- 水平偏移: " << OFFSET_X << std::endl;
        std::wcout << L"- 垂直偏移: " << OFFSET_Y << std::endl;
    }

    // 创建托盘消息窗口
    bool CreateTrayWindow() {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = TrayWindowProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = L"WeChatMaskTrayWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        if (RegisterClassEx(&wc) == 0) {
            return false;
        }

        m_trayHwnd = CreateWindowEx(
            0,
            L"WeChatMaskTrayWindow",
            L"WeChat Mask Tray Window",
            0,
            0, 0, 0, 0,
            nullptr,
            nullptr,
            m_hInstance,
            this
        );

        return m_trayHwnd != nullptr;
    }

    // 托盘窗口过程函数
    static LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        WeChatMask *pThis = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
            pThis = reinterpret_cast<WeChatMask *>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<WeChatMask *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleTrayMessage(hwnd, uMsg, wParam, lParam);
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // 处理托盘消息
    LRESULT HandleTrayMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_TRAYICON) {
            if (lParam == WM_RBUTTONUP) {
                ShowTrayMenu();
                return 0;
            }
        } else if (uMsg == WM_COMMAND) {
            if (LOWORD(wParam) == IDM_EXIT) {
                PostQuitMessage(0);
                return 0;
            }
        } else if (uMsg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // 添加托盘图标
    bool AddTrayIcon() {
        m_trayData.cbSize = sizeof(NOTIFYICONDATA);
        m_trayData.hWnd = m_trayHwnd;
        m_trayData.uID = ID_TRAYICON;
        m_trayData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_trayData.uCallbackMessage = WM_TRAYICON;
        m_trayData.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wcscpy_s(m_trayData.szTip, L"微信遮罩程序");

        return Shell_NotifyIcon(NIM_ADD, &m_trayData) != FALSE;
    }

    // 显示托盘菜单
    void ShowTrayMenu() {
        HMENU hMenu = CreatePopupMenu();

        if (hMenu) {
            AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出");

            POINT pt;
            GetCursorPos(&pt);

            SetForegroundWindow(m_trayHwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_trayHwnd, nullptr);
            PostMessage(m_trayHwnd, WM_NULL, 0, 0);

            DestroyMenu(hMenu);
        }
    }

    // 获取进程ID by 进程名
    DWORD GetProcessIdByName(const wchar_t *processName) {
        DWORD processId = 0;
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        if (hSnapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32W);

            if (Process32FirstW(hSnapshot, &pe32)) {
                do {
                    if (wcscmp(pe32.szExeFile, processName) == 0) {
                        processId = pe32.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }

        return processId;
    }

    // 通过进程名查找微信窗口
    bool FindWeChatWindow() {
        m_wechatProcessId = GetProcessIdByName(L"WeChat.exe");
        if (m_wechatProcessId == 0) {
            std::wcout << L"未找到 WeChat.exe 进程" << std::endl;
            return false;
        }

        std::wcout << L"找到微信进程，PID: " << m_wechatProcessId << std::endl;

        // 枚举所有窗口，找到属于微信进程的主窗口
        HWND hwnd = nullptr;
        do {
            hwnd = FindWindowEx(nullptr, hwnd, nullptr, nullptr);
            if (hwnd) {
                DWORD processId = 0;
                GetWindowThreadProcessId(hwnd, &processId);

                if (processId == m_wechatProcessId && IsWindowVisible(hwnd)) {
                    // 检查窗口大小，确保是主窗口
                    RECT rect;
                    GetWindowRect(hwnd, &rect);
                    int width = rect.right - rect.left;
                    int height = rect.bottom - rect.top;

                    if (width > 400 && height > 300) {
                        m_wechatHwnd = hwnd;

                        // 输出窗口信息
                        wchar_t title[256];
                        GetWindowTextW(hwnd, title, 256);
                        std::wcout << L"找到微信窗口: " << title << std::endl;
                        std::wcout << L"窗口大小: " << width << "x" << height << std::endl;
                        std::wcout << L"窗口位置: (" << rect.left << ", " << rect.top << ")" << std::endl;

                        return true;
                    }
                }
            }
        } while (hwnd);

        std::wcout << L"未找到微信主窗口" << std::endl;
        return false;
    }

    // 检查微信窗口是否可见
    bool IsWeChatWindowVisible() {
        if (!m_wechatHwnd) return false;
        return IsWindowVisible(m_wechatHwnd) && !IsIconic(m_wechatHwnd);
    }

    // 检查微信窗口是否存在
    bool IsWeChatWindowExists() {
        if (!m_wechatHwnd) return false;
        return IsWindow(m_wechatHwnd);
    }

    // 创建遮罩窗口
    bool CreateMaskWindow() {
        // 获取微信窗口位置
        RECT wechatRect;
        if (!GetWindowRect(m_wechatHwnd, &wechatRect)) {
            std::wcout << L"获取微信窗口位置失败" << std::endl;
            return false;
        }

        // 计算遮罩位置
        int maskX = wechatRect.left + OFFSET_X;
        int maskY = wechatRect.top + OFFSET_Y;

        std::wcout << L"遮罩位置: (" << maskX << ", " << maskY << ")" << std::endl;
        std::wcout << L"遮罩大小: " << MASK_WIDTH << "x" << MASK_HEIGHT << std::endl;

        // 注册窗口类
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = StaticMaskWindowProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = L"WeChatMaskClass";
        wc.hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); // 白色背景
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        RegisterClassEx(&wc);

        // 创建窗口 - 使用Owner窗口而不是子窗口
        m_maskHwnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            L"WeChatMaskClass",
            L"WeChat Mask",
            WS_POPUP | WS_VISIBLE,
            maskX,
            maskY,
            MASK_WIDTH,
            MASK_HEIGHT,
            m_wechatHwnd, // 设置Owner为微信窗口
            nullptr,
            m_hInstance,
            this
        );

        if (!m_maskHwnd) {
            std::wcout << L"创建遮罩窗口失败: " << GetLastError() << std::endl;
            return false;
        }

        // 设置不透明度
        SetLayeredWindowAttributes(m_maskHwnd, 0, MASK_OPACITY, LWA_ALPHA);

        // 设置窗口在Z序中的位置 - 在微信窗口之上但不在最顶层
        SetWindowPos(
            m_maskHwnd,
            m_wechatHwnd, // 放在微信窗口之上
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
        );

        // 显示窗口
        ShowWindow(m_maskHwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(m_maskHwnd);

        std::wcout << L"遮罩窗口创建成功" << std::endl;
        std::wcout << L"窗口句柄: " << m_maskHwnd << std::endl;
        std::wcout << L"Owner窗口句柄: " << m_wechatHwnd << std::endl;

        return true;
    }

    // 静态窗口过程函数
    static LRESULT CALLBACK StaticMaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        WeChatMask *pThis = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
            pThis = reinterpret_cast<WeChatMask *>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<WeChatMask *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->MaskWindowProc(hwnd, uMsg, wParam, lParam);
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // 成员窗口过程函数
    LRESULT MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                // 绘制白色背景
                RECT rect;
                GetClientRect(hwnd, &rect);
                HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);

                // 绘制红色边框用于调试
                HPEN hPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
                HPEN hOldPen = (HPEN) SelectObject(hdc, hPen);
                HBRUSH hNullBrush = (HBRUSH) GetStockObject(NULL_BRUSH);
                HBRUSH hOldBrush = (HBRUSH) SelectObject(hdc, hNullBrush);

                Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);

                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hPen);

                // 绘制文字
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(0, 0, 0));

                std::wstring text = L"遮罩大小 " + std::to_wstring(MASK_WIDTH) + L"x" + std::to_wstring(MASK_HEIGHT);
                TextOutW(hdc, 10, 10, text.c_str(), (int) text.length());

                std::wstring text2 = L"遮罩偏移 " + std::to_wstring(OFFSET_X) + L"x" + std::to_wstring(OFFSET_Y);
                TextOutW(hdc, 10, 30, text2.c_str(), (int) text2.length());

                std::wstring text3 = L"遮罩透明 " + std::to_wstring(MASK_OPACITY);
                TextOutW(hdc, 10, 50, text3.c_str(), (int) text3.length());

                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_ERASEBKGND:
                return 1; // 防止闪烁

            case WM_DESTROY:
                return 0;

            // 处理鼠标消息，使其穿透到下层窗口
            case WM_NCHITTEST:
                return HTTRANSPARENT;
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // 更新遮罩位置和可见性
    void UpdateMask() {
        if (!m_wechatHwnd || !m_maskHwnd) return;

        // 检查微信窗口是否存在且可见
        bool wechatExists = IsWeChatWindowExists();
        bool wechatVisible = wechatExists && IsWeChatWindowVisible();

        if (wechatExists && wechatVisible) {
            // 微信窗口存在且可见，更新遮罩位置并显示
            RECT wechatRect;
            if (GetWindowRect(m_wechatHwnd, &wechatRect)) {
                int newX = wechatRect.left + OFFSET_X;
                int newY = wechatRect.top + OFFSET_Y;

                // 移动遮罩窗口
                SetWindowPos(
                    m_maskHwnd,
                    HWND_TOP,
                    newX,
                    newY,
                    MASK_WIDTH,
                    MASK_HEIGHT,
                    SWP_NOACTIVATE | SWP_NOZORDER
                );

                // 显示遮罩窗口
                if (!IsWindowVisible(m_maskHwnd)) {
                    ShowWindow(m_maskHwnd, SW_SHOWNOACTIVATE);
                }
            }
        } else {
            // 微信窗口不存在或不可见，隐藏遮罩窗口
            if (IsWindowVisible(m_maskHwnd)) {
                ShowWindow(m_maskHwnd, SW_HIDE);
            }

            // 如果微信窗口不存在，清理资源
            if (!wechatExists) {
                std::wcout << L"微信窗口已关闭，清理遮罩资源" << std::endl;
                if (m_maskHwnd) {
                    DestroyWindow(m_maskHwnd);
                    m_maskHwnd = nullptr;
                }
                m_wechatHwnd = nullptr;
            }
        }
    }

    // 重新加载配置
    void ReloadConfig() {
        LoadConfigFromIni();
        std::wcout << L"配置已重新加载" << std::endl;

        // 如果遮罩窗口存在，更新其大小和位置
        if (m_maskHwnd && m_wechatHwnd && IsWeChatWindowExists()) {
            RECT wechatRect;
            if (GetWindowRect(m_wechatHwnd, &wechatRect)) {
                int newX = wechatRect.left + OFFSET_X;
                int newY = wechatRect.top + OFFSET_Y;

                static DWORD lastDebugOutput = 0;
                DWORD currentTime = GetTickCount();
                if (currentTime - lastDebugOutput > 1000) {
                    std::wcout << L"更新遮罩位置: (" << newX << ", " << newY << ")" << std::endl;
                    lastDebugOutput = currentTime;
                }

                // 移动遮罩窗口
                SetWindowPos(
                    m_maskHwnd,
                    m_wechatHwnd, // 保持相对于微信窗口的Z序
                    newX,
                    newY,
                    MASK_WIDTH,
                    MASK_HEIGHT,
                    SWP_NOACTIVATE | SWP_NOZORDER
                );

                // 更新不透明度
                SetLayeredWindowAttributes(m_maskHwnd, 0, MASK_OPACITY, LWA_ALPHA);
            }
        }
    }

    // 主循环
    void Run() {
        std::wcout << L"微信遮罩程序已启动" << std::endl;
        std::wcout << L"等待微信窗口..." << std::endl;

        // 创建托盘窗口
        if (!CreateTrayWindow()) {
            return;
        }

        // 添加托盘图标
        if (!AddTrayIcon()) {
            DestroyWindow(m_trayHwnd);
            m_trayHwnd = nullptr;
            return;
        }

        // 主消息循环
        MSG msg = {};
        DWORD lastUpdateTime = GetTickCount();
        const DWORD updateInterval = 50; // 50毫秒更新一次
        bool wechatFound = false;

        while (true) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    break;
                }

                if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                    break;
                }

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                DWORD currentTime = GetTickCount();
                if (currentTime - lastUpdateTime >= updateInterval) {
                    if (!wechatFound) {
                        // 尝试查找微信窗口
                        wechatFound = FindWeChatWindow();
                        if (wechatFound) {
                            std::wcout << L"找到微信窗口，创建遮罩..." << std::endl;
                            if (CreateMaskWindow()) {
                                std::wcout << L"遮罩创建成功" << std::endl;
                            } else {
                                std::wcout << L"遮罩创建失败" << std::endl;
                                wechatFound = false;
                            }
                        }
                    } else {
                        // 更新遮罩位置和可见性
                        UpdateMask();

                        // 如果微信窗口不存在，重置状态
                        if (!IsWeChatWindowExists()) {
                            std::wcout << L"微信窗口已关闭，等待重新打开..." << std::endl;
                            wechatFound = false;
                        }
                    }
                    lastUpdateTime = currentTime;
                }
                Sleep(1); // 避免占用100%CPU
            }
        }

        // 清理资源
        if (m_maskHwnd) {
            DestroyWindow(m_maskHwnd);
            m_maskHwnd = nullptr;
        }

        std::wcout << L"程序已退出" << std::endl;
    }
};

// 隐藏控制台窗口
void HideConsoleWindow() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        ShowWindow(hwnd, SW_HIDE);
    }
}

int main() {
    std::wcout << L"微信遮罩程序 (智能隐藏版本)" << std::endl;
    std::wcout << L"========================" << std::endl;
    std::wcout << L"特性:" << std::endl;
    std::wcout << L"- 从settings.ini文件加载配置" << std::endl;
    std::wcout << L"- 遮罩位于微信窗口之上" << std::endl;
    std::wcout << L"- 微信窗口隐藏时遮罩也隐藏" << std::endl;
    std::wcout << L"- 自动跟随微信窗口移动" << std::endl;
    std::wcout << L"- 不会遮挡其他应用程序" << std::endl;
    std::wcout << L"- 鼠标事件穿透" << std::endl;
    std::wcout << L"========================" << std::endl;

    HideConsoleWindow();

    WeChatMask mask;
    mask.Run();

    return 0;
}
