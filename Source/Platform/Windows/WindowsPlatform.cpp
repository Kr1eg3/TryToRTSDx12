#include "WindowsPlatform.h"
#include <codecvt>
#include <locale>

namespace Platform {

String WStringToString(const WString& wstr) {
    if (wstr.empty()) return String();

    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                   nullptr, 0, nullptr, nullptr);
    String result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                        &result[0], size, nullptr, nullptr);
    return result;
}

WString StringToWString(const String& str) {
    if (str.empty()) return WString();

    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(),
                                   nullptr, 0);
    WString result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(),
                        &result[0], size);
    return result;
}

void ShowMessageBox(const String& title, const String& message) {
    WString wTitle = StringToWString(title);
    WString wMessage = StringToWString(message);
    MessageBoxW(nullptr, wMessage.c_str(), wTitle.c_str(), MB_OK | MB_ICONINFORMATION);
}

void OutputDebugMessage(const String& message) {
    WString wMessage = StringToWString(message);
    OutputDebugStringW(wMessage.c_str());
}

} // namespace Platform