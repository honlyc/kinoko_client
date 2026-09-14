#include "pch.h"
#include "hook.h"
#include "config.h"
#include "debug.h"
#include "ztl/ztl.h"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

#define REPLACE_STRING(INDEX, NEW_STRING) \
    do { \
        static char sEncoded[GetLength(NEW_STRING) + 2]; \
        EncodeString(INDEX, NEW_STRING, sEncoded); \
    } while (0)


class StringPool {
public:
    class Key {
    public:
        ZArray<uint8_t> m_aKey;
    };
    static_assert(sizeof(Key) == 0x4);
};

static auto StringPool__Key__ctor = reinterpret_cast<void (__thiscall*)(StringPool::Key*, const uint8_t*, uint32_t, uint32_t)>(0x00746470);

static auto StringPool__ms_aKey = reinterpret_cast<const uint8_t*>(0x00B98830);
static auto StringPool__ms_aString = reinterpret_cast<const char**>(0x00C5A878);

constexpr size_t GetLength(const char* s) {
    size_t n = 0;
    while (s[n]) {
        ++n;
    }
    return n;
}

void EncodeString(int nIdx, const char* sSource, char* sDestination) {
    StringPool::Key keygen;
    StringPool__Key__ctor(&keygen, StringPool__ms_aKey, 0x10, 0);
    size_t n = strlen(sSource);
    for (size_t i = 0; i < n; ++i) {
        uint8_t key = keygen.m_aKey[i % 0x10];
        sDestination[i + 1] = sSource[i] ^ key;
        if (static_cast<uint8_t>(sSource[i]) == static_cast<uint8_t>(key)) {
            sDestination[i + 1] = key;
        }
    }
    sDestination[0] = 0;
    sDestination[n + 1] = 0;
    StringPool__ms_aString[nIdx] = sDestination;
}


// Persistent encoded buffers to ensure pointers remain valid for the process lifetime.
static std::vector<std::vector<char>> g_aEncodedBuffers;

static void ReplaceStringRuntime(int nIdx, const std::string& sSource) {
    if (nIdx < 0) {
        return;
    }
    if (g_aEncodedBuffers.size() <= static_cast<size_t>(nIdx)) {
        g_aEncodedBuffers.resize(nIdx + 1);
    }
    // EncodeString writes n+2 bytes
    g_aEncodedBuffers[nIdx].assign(sSource.size() + 2, 0);
    EncodeString(nIdx, sSource.c_str(), g_aEncodedBuffers[nIdx].data());
}

static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

static inline void trim(std::string& s) {
    ltrim(s);
    rtrim(s);
}

// Process escape sequences in a value read from the translations file.
// Supports: \n \r \t \\ \0
static std::string UnescapeString(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n':  result += '\n'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case '0':  result += '\0'; ++i; break;
                default:   result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

// The client expects GBK-encoded strings; the translations file is UTF-8.
// If the input is not valid UTF-8, assume it is already GBK and pass it through.
static std::string Utf8ToGbk(const std::string& s) {
    if (s.empty()) {
        return s;
    }
    int nWide = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (nWide <= 0) {
        return s;
    }
    std::wstring sWide(nWide, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), sWide.data(), nWide);
    int nGbk = WideCharToMultiByte(936, 0, sWide.data(), nWide, nullptr, 0, nullptr, nullptr);
    if (nGbk <= 0) {
        return s;
    }
    std::string sGbk(nGbk, 0);
    WideCharToMultiByte(936, 0, sWide.data(), nWide, sGbk.data(), nGbk, nullptr, nullptr);
    return sGbk;
}

// Translations file format: UTF-8, one "index=translation" per line.
// Lines starting with '#' are comments. Use \r\n escape sequences for line breaks.
static void LoadTranslationsFromFile(const char* sPath) {
    std::ifstream ifs(sPath, std::ios::binary);
    if (!ifs) {
        DEBUG_MESSAGE("LoadTranslationsFromFile: failed to open %s", sPath);
        return;
    }
    std::string sLine;
    bool bFirstLine = true;
    unsigned int nLine = 0;
    unsigned int nLoaded = 0;
    while (std::getline(ifs, sLine)) {
        ++nLine;
        // strip UTF-8 BOM on the first line
        if (bFirstLine) {
            bFirstLine = false;
            if (sLine.size() >= 3 &&
                static_cast<uint8_t>(sLine[0]) == 0xEF &&
                static_cast<uint8_t>(sLine[1]) == 0xBB &&
                static_cast<uint8_t>(sLine[2]) == 0xBF) {
                sLine.erase(0, 3);
            }
        }
        // std::getline strips '\n' but keeps '\r' on CRLF files
        if (!sLine.empty() && sLine.back() == '\r') {
            sLine.pop_back();
        }
        if (sLine.empty() || sLine[0] == '#') {
            continue;
        }
        size_t nPos = sLine.find('=');
        if (nPos == std::string::npos) {
            continue;
        }
        std::string sKey = sLine.substr(0, nPos);
        std::string sValue = sLine.substr(nPos + 1);
        trim(sKey);
        if (sKey.empty()) {
            continue;
        }
        try {
            int nIdx = std::stoi(sKey);
            ReplaceStringRuntime(nIdx, Utf8ToGbk(UnescapeString(sValue)));
            ++nLoaded;
        } catch (...) {
            DEBUG_MESSAGE("LoadTranslationsFromFile: invalid key on line %u: %s", nLine, sKey.c_str());
        }
    }
    DEBUG_MESSAGE("LoadTranslationsFromFile: loaded %u strings from %s", nLoaded, sPath);
}


void AttachStringPoolMod() {
    REPLACE_STRING(2585, CONFIG_REGISTRY_KEY);
    REPLACE_STRING(2948, CONFIG_VERSION_STRING);

    LoadTranslationsFromFile("translations/zh_CN.txt");
}
