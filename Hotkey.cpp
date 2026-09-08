#include "Hotkey.h"
#include <cstdio>

const char* HK_KEY_NAMES[7] = {
    "playpause", "prev", "next", "volup", "voldn", "restore", "minimize"
};

// Key name lookup tables
static const struct { int vk; const wchar_t* name; } VK_NAMES[] = {
    { VK_LEFT,  L"Left"  }, { VK_RIGHT, L"Right" },
    { VK_UP,    L"Up"    }, { VK_DOWN,  L"Down"  },
    { VK_SPACE, L"Space" }, { VK_RETURN,L"Enter" },
    { VK_TAB,   L"Tab"   }, { VK_DELETE,L"Del"   },
    { VK_ESCAPE,L"Esc"   }, { VK_BACK,  L"Back"  },
    { VK_HOME,  L"Home"  }, { VK_END,   L"End"   },
    { VK_PRIOR, L"PgUp"  }, { VK_NEXT,  L"PgDn"  },
    { VK_OEM_PLUS, L"+"  }, { VK_OEM_MINUS, L"-" },
    { 0, NULL }
};

static std::wstring VkToName(int vk) {
    for (auto& e : VK_NAMES) { if (e.vk == vk) return e.name; }
    if (vk >= '0' && vk <= '9') return std::wstring(1, (wchar_t)vk);
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)vk);
    wchar_t buf[16]; swprintf(buf, 16, L"VK_%d", vk); return buf;
}

static int NameToVk(const std::wstring& name) {
    for (auto& e : VK_NAMES) { if (name == e.name) return e.vk; }
    if (name.size() == 1) {
        wchar_t c = name[0];
        if (c >= '0' && c <= '9') return (int)c;
        if (c >= 'A' && c <= 'Z') return (int)c;
        if (c >= 'a' && c <= 'z') return (int)(c - 32);
    }
    return 0;
}

std::wstring HotkeyToString(int vk, int mod) {
    std::wstring s;
    if (mod & MOD_CONTROL) s += L"Ctrl+";
    if (mod & MOD_ALT)    s += L"Alt+";
    s += VkToName(vk);
    return s;
}

std::wstring BindingToCode(int vk, int mod) {
    std::wstring s;
    if (mod & MOD_CONTROL) s += L"C";
    if (mod & MOD_ALT)    s += L"A";
    if (!s.empty()) s += L"+";
    s += VkToName(vk);
    return s;
}

bool CodeToBinding(const std::wstring& code, int& vk, int& mod) {
    vk = 0; mod = 0;
    size_t pos = code.find(L'+');
    if (pos == std::wstring::npos) return false;
    std::wstring modPart = code.substr(0, pos);
    std::wstring keyPart = code.substr(pos + 1);
    for (auto c : modPart) {
        if (c == L'C' || c == L'c') mod |= MOD_CONTROL;
        if (c == L'A' || c == L'a') mod |= MOD_ALT;
    }
    vk = NameToVk(keyPart);
    return vk != 0;
}
