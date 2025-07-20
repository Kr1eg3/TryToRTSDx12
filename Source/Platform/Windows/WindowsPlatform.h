#pragma once

#include "../../Core/Utilities/Types.h"

// Windows includes
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

// DirectX 12 includes
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

// ComPtr
#include <wrl/client.h>
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// Platform-specific types
using WindowHandle = HWND;
using InstanceHandle = HINSTANCE;

// Error handling
class WindowsException {
public:
    WindowsException(HRESULT hr, const String& function, const String& file, int line)
        : m_hr(hr), m_function(function), m_file(file), m_line(line) {}

    String GetMessage() const {
        LPWSTR messageBuffer = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, m_hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&messageBuffer, 0, nullptr
        );

        String message = "Windows Error in " + m_function + "\n";
        message += "File: " + m_file + "\n";
        message += "Line: " + std::to_string(m_line) + "\n";
        message += "HRESULT: 0x" + std::to_string(m_hr) + "\n";

        if (messageBuffer) {
            // Convert wide string to string (simplified)
            int size = WideCharToMultiByte(CP_UTF8, 0, messageBuffer, -1, nullptr, 0, nullptr, nullptr);
            String errorText(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, messageBuffer, -1, &errorText[0], size, nullptr, nullptr);
            message += "Description: " + errorText;
            LocalFree(messageBuffer);
        }

        return message;
    }

private:
    HRESULT m_hr;
    String m_function;
    String m_file;
    int m_line;
};

// Error checking macros
#define THROW_IF_FAILED(hr, function) \
    do { \
        HRESULT _hr = (hr); \
        if (FAILED(_hr)) { \
            throw WindowsException(_hr, function, __FILE__, __LINE__); \
        } \
    } while(0)

#define CHECK_WIN32_BOOL(result, function) \
    do { \
        if (!(result)) { \
            THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()), function); \
        } \
    } while(0)

// Utility functions
namespace Platform {
    String WStringToString(const WString& wstr);
    WString StringToWString(const String& str);

    void ShowMessageBox(const String& title, const String& message);
    void OutputDebugMessage(const String& message);
}