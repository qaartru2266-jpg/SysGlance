#pragma once

#include "common.h"

namespace sysglance {

class ConfigService {
public:
    AppConfig Load() const;
    bool Save(const AppConfig& config) const;
    void Normalize(AppConfig& config) const;
    AppConfig RecommendedHud(const AppConfig& base) const;
    bool LoadLastGoodHud(AppConfig& config) const;
    bool SaveLastGoodHud(const AppConfig& config) const;
    std::wstring Path() const;

private:
    std::wstring ConfigDirectory() const;
    static int ReadInt(const std::wstring& path, const wchar_t* section,
                       const wchar_t* key, int fallback);
    static bool ReadBool(const std::wstring& path, const wchar_t* section,
                         const wchar_t* key, bool fallback);
    static std::uint64_t ReadUInt64(const std::wstring& path, const wchar_t* section,
                                    const wchar_t* key, std::uint64_t fallback);
};

bool SetAutoStart(bool enabled);

}  // namespace sysglance
