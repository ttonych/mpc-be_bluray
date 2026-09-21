// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "DSUtil/Profile.h"
#include "BlurayAdvancedSettings.h"

struct BluraySettings {
    bool menus = false;
    int region = 2;
    CStringW country;
    CStringW menuLanguage = L"eng", audioLanguage = L"eng", subtitleLanguage = L"eng";
    bool persistent = true;
    BlurayAdvanced::Values advanced{};
    CStringW javaHome, persistentRoot, cacheRoot;

    static bool IsCode(const CStringW& value, int length) {
        if (value.IsEmpty()) return true;
        if (value.GetLength() != length) return false;
        for (int i = 0; i < length; ++i)
            if (!((value[i] >= L'a' && value[i] <= L'z') || (value[i] >= L'A' && value[i] <= L'Z'))) return false;
        return true;
    }
    void Load() {
        auto& p = AfxGetProfile();
        p.ReadBool(L"Settings", L"BluRayMenus", menus);
        p.ReadInt(L"Settings", L"BluRayRegion", region);
        if (region != 1 && region != 2 && region != 4) region = 2;
        p.ReadString(L"Settings", L"BluRayCountry", country);
        country.Trim(); country.MakeUpper();
        if (!IsCode(country, 2)) country.Empty();
        p.ReadString(L"Settings", L"BluRayMenuLanguage", menuLanguage);
        p.ReadString(L"Settings", L"BluRayAudioLanguage", audioLanguage);
        p.ReadString(L"Settings", L"BluRaySubtitleLanguage", subtitleLanguage);
        for (auto* language : {&menuLanguage, &audioLanguage, &subtitleLanguage}) {
            language->Trim(); language->MakeLower();
            if (!IsCode(*language, 3)) *language = L"eng";
        }
        p.ReadBool(L"Settings", L"BluRayPersistentStorage", persistent);
        for (size_t i = 0; i < advanced.size(); ++i) {
            unsigned n = 0;
            advanced[i].enabled = p.ReadUInt(L"Settings", BlurayAdvanced::Specs[i].key, n) && BlurayAdvanced::Valid(i, n);
            advanced[i].number = n;
        }
        p.ReadString(L"Settings", L"BluRayJavaHome", javaHome);
        p.ReadString(L"Settings", L"BluRayPersistentRoot", persistentRoot);
        p.ReadString(L"Settings", L"BluRayCacheRoot", cacheRoot);
    }
    void Save() const {
        auto& p = AfxGetProfile();
        p.WriteBool(L"Settings", L"BluRayMenus", menus);
        p.WriteInt(L"Settings", L"BluRayRegion", region);
        p.WriteString(L"Settings", L"BluRayCountry", country);
        p.WriteString(L"Settings", L"BluRayMenuLanguage", menuLanguage);
        p.WriteString(L"Settings", L"BluRayAudioLanguage", audioLanguage);
        p.WriteString(L"Settings", L"BluRaySubtitleLanguage", subtitleLanguage);
        p.WriteBool(L"Settings", L"BluRayPersistentStorage", persistent);
        for (size_t i = 0; i < advanced.size(); ++i) {
            if (advanced[i].enabled) p.WriteUInt(L"Settings", BlurayAdvanced::Specs[i].key, advanced[i].number);
            else p.DeleteValue(L"Settings", BlurayAdvanced::Specs[i].key);
        }
        p.WriteString(L"Settings", L"BluRayJavaHome", javaHome);
        p.WriteString(L"Settings", L"BluRayPersistentRoot", persistentRoot);
        p.WriteString(L"Settings", L"BluRayCacheRoot", cacheRoot);
    }
};
