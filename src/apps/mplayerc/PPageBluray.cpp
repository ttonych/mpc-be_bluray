// SPDX-License-Identifier: GPL-3.0-or-later
#include "stdafx.h"
#include "PPageBluray.h"
#include "MainFrm.h"
#include "BlurayMenu.h"
#include "BlurayDiscStorage.h"
#include "BlurayDialogs.h"
#include "DSUtil/ISOLang.h"

IMPLEMENT_DYNAMIC(CPPageBluray, CPPageBase)
CPPageBluray::CPPageBluray() : CPPageBase(IDD, IDD),
    m_java(m_settings, [this]() { SetModified(); }),
    m_compatibility(m_settings, [this]() { SetModified(); }) {}

BEGIN_MESSAGE_MAP(CPPageBluray, CPPageBase)
    ON_CONTROL_RANGE(CBN_EDITCHANGE, IDC_BD_COUNTRY, IDC_BD_SUB_LANG, OnCodeEdited)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_BD_COUNTRY, IDC_BD_SUB_LANG, OnCodeEdited)
    ON_CBN_SELCHANGE(IDC_BD_REGION, OnRegionChanged)
    ON_CONTROL_RANGE(CBN_EDITCHANGE, IDC_BD_AGE, IDC_BD_OUTPUT, OnCodeEdited)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_BD_AGE, IDC_BD_OUTPUT, OnCodeEdited)
    ON_NOTIFY(TCN_SELCHANGING, IDC_BD_TABS, OnTabChanging)
    ON_NOTIFY(TCN_SELCHANGE, IDC_BD_TABS, OnTabChanged)
END_MESSAGE_MAP()

void CPPageBluray::OnCodeEdited(UINT) {
    SetModified();
}
void CPPageBluray::OnRegionChanged() {
    const int region=m_region.GetCurSel();
    if (region>=0 && region<3) SetDlgItemTextW(IDC_BD_REGION_HINT,ResStr(IDS_BD_REGION_A_HINT+region));
    SetModified();
}

void CPPageBluray::DoDataExchange(CDataExchange* pDX) {
    __super::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_BD_MENUS, m_menus);
    DDX_Control(pDX, IDC_BD_TABS, m_tabs);
    DDX_Control(pDX, IDC_BD_AGE, m_age);
    DDX_Control(pDX, IDC_BD_OUTPUT, m_output);
    DDX_Control(pDX, IDC_BD_REGION, m_region);
    DDX_Control(pDX, IDC_BD_COUNTRY, m_country);
    DDX_Control(pDX, IDC_BD_MENU_LANG, m_menuLanguage);
    DDX_Control(pDX, IDC_BD_AUDIO_LANG, m_audioLanguage);
    DDX_Control(pDX, IDC_BD_SUB_LANG, m_subtitleLanguage);
}

void CPPageBluray::FillCombo(CComboBox& combo, const std::vector<std::pair<CStringW, CStringW>>& choices, const CStringW& value) {
    for (const auto& choice : choices) combo.AddString(choice.second);
    for (size_t i = 0; i < choices.size(); ++i) if (value.CompareNoCase(choices[i].first) == 0) {
        combo.SetCurSel(int(i)); return;
    }
    combo.SetWindowTextW(value);
}

BOOL CPPageBluray::OnInitDialog() {
    __super::OnInitDialog();
    m_settings.Load();
    const auto& s = m_settings;
    m_menus = s.menus;
    UpdateData(FALSE);
    AddStringData(m_region, L"A", 1);
    AddStringData(m_region, L"B", 2);
    AddStringData(m_region, L"C", 4);
    SelectByItemData(m_region, s.region);
    SetDlgItemTextW(IDC_BD_REGION_HINT,ResStr(IDS_BD_REGION_A_HINT+m_region.GetCurSel()));

    m_countries.emplace_back(L"", ResStr(IDS_BD_NO_PREFERENCE));
    EnumSystemLocalesEx([](LPWSTR locale, DWORD, LPARAM parameter) -> BOOL {
        auto& countries = *reinterpret_cast<std::vector<std::pair<CStringW, CStringW>>*>(parameter);
        wchar_t code[8]{}, name[160]{};
        if (GetLocaleInfoEx(locale, LOCALE_SISO3166CTRYNAME, code, 8) && wcslen(code) == 2 && BluraySettings::IsCode(code, 2)
            && GetLocaleInfoEx(locale, LOCALE_SLOCALIZEDCOUNTRYNAME, name, 160)) {
            const CStringW iso(code);
            if (std::none_of(countries.begin(), countries.end(), [&](const auto& c) { return c.first == iso; }))
                countries.emplace_back(iso, CStringW(name));
        }
        return TRUE;
    }, LOCALE_WINDOWS, reinterpret_cast<LPARAM>(&m_countries), nullptr);
    std::sort(m_countries.begin() + 1, m_countries.end(), [](const auto& a, const auto& b) { return a.second.CompareNoCase(b.second) < 0; });
    FillCombo(m_country, m_countries, s.country);
    m_country.LimitText(160);

    m_languages.emplace_back(L"", ResStr(IDS_BD_NO_PREFERENCE));
    const char* codes[] = {"eng", "rus", "ukr", "bel", "fra", "deu", "spa", "ita", "por", "pol", "ces", "slk", "slv", "hrv", "srp", "bul", "ron", "hun", "ell", "nld", "dan", "swe", "nor", "fin", "isl", "est", "lav", "lit", "tur", "ara", "heb", "hin", "jpn", "kor", "zho", "tha", "vie", "ind", "msa"};
    for (const auto code : codes) {
        const CStringW iso(code);
        CStringW name = ISO6392ToLanguage(code);
        wchar_t localized[160]{};
        const auto lcid = ISO6392ToLcid(code);
        if (lcid && GetLocaleInfoW(lcid, LOCALE_SLOCALIZEDLANGUAGENAME, localized, 160)) name = localized;
        m_languages.emplace_back(iso, name);
    }
    FillCombo(m_menuLanguage, m_languages, s.menuLanguage);
    FillCombo(m_audioLanguage, m_languages, s.audioLanguage);
    FillCombo(m_subtitleLanguage, m_languages, s.subtitleLanguage);
    for (auto* combo : {&m_menuLanguage, &m_audioLanguage, &m_subtitleLanguage}) combo->LimitText(160);

    BlurayUi::FillValue(m_age, BlurayAdvanced::Age, s.advanced[BlurayAdvanced::Age]);
    BlurayUi::FillValue(m_output, BlurayAdvanced::Output, s.advanced[BlurayAdvanced::Output]);
    // The first tab is laid out in the property page resource. Keep its native
    // static labels and groups together with its controls when changing tabs.
    for (CWnd* child = GetWindow(GW_CHILD); child; child = child->GetWindow(GW_HWNDNEXT)) {
        const int id = child->GetDlgCtrlID();
        if (id != IDC_BD_MENUS && id != IDC_BD_TABS && id != IDC_BD_REOPEN_NOTE)
            m_menuControls.push_back(child->m_hWnd);
    }
    m_tabs.InsertItem(0, ResStr(IDS_BD_TAB_MENU));
    m_tabs.InsertItem(1, ResStr(IDS_BD_TAB_JAVA));
    m_tabs.InsertItem(2, ResStr(IDS_BD_COMPATIBILITY));
    m_tabs.SetCurSel(0);
    m_java.Create(IDD_BD_JAVA, this);
    m_compatibility.Create(IDD_BD_ADVANCED, this);
    CRect rect; m_tabs.GetClientRect(rect); m_tabs.AdjustRect(FALSE, rect);
    m_tabs.MapWindowPoints(this, rect);
    m_java.MoveWindow(rect); m_compatibility.MoveWindow(rect);
    ShowTab(0);
    return TRUE;
}

bool CPPageBluray::ReadCode(CComboBox& combo, const std::vector<std::pair<CStringW, CStringW>>& choices, int length, CStringW& code) {
    combo.GetWindowTextW(code); code.Trim();
    for (const auto& choice : choices) if (code.CompareNoCase(choice.second) == 0) { code = choice.first; break; }
    if (length == 2) code.MakeUpper(); else code.MakeLower();
    if (!BluraySettings::IsCode(code, length)) {
        AfxMessageBox(ResStr(length == 2 ? IDS_BD_BAD_COUNTRY : IDS_BD_BAD_LANGUAGE), MB_ICONEXCLAMATION);
        combo.SetFocus(); return false;
    }
    return true;
}

void CPPageBluray::ShowTab(int tab) {
    m_tabs.SetCurSel(tab);
    for (HWND window : m_menuControls) ::ShowWindow(window, tab == 0 ? SW_SHOW : SW_HIDE);
    m_java.ShowWindow(tab == 1 ? SW_SHOW : SW_HIDE);
    m_compatibility.ShowWindow(tab == 2 ? SW_SHOW : SW_HIDE);
}

bool CPPageBluray::CommitMenu() {
    BluraySettings s = m_settings;
    s.region = int(m_region.GetItemData(m_region.GetCurSel()));
    if (!ReadCode(m_country, m_countries, 2, s.country)
        || !ReadCode(m_menuLanguage, m_languages, 3, s.menuLanguage)
        || !ReadCode(m_audioLanguage, m_languages, 3, s.audioLanguage)
        || !ReadCode(m_subtitleLanguage, m_languages, 3, s.subtitleLanguage)
        || !BlurayUi::ReadValue(m_age, BlurayAdvanced::Age, s.advanced[BlurayAdvanced::Age])
        || !BlurayUi::ReadValue(m_output, BlurayAdvanced::Output, s.advanced[BlurayAdvanced::Output])) return false;
    m_settings = s; return true;
}

void CPPageBluray::OnTabChanging(NMHDR*, LRESULT* result) {
    *result = m_tabs.GetCurSel() == 0 ? !CommitMenu() : !m_compatibility.CommitValue();
}
void CPPageBluray::OnTabChanged(NMHDR*, LRESULT* result) { *result = 0; ShowTab(m_tabs.GetCurSel()); }
BOOL CPPageBluray::OnKillActive() {
    if (!m_compatibility.CommitValue() || (m_tabs.GetCurSel() == 0 && !CommitMenu())) return FALSE;
    return __super::OnKillActive();
}
BOOL CPPageBluray::OnApply() {
    if (!UpdateData() || !m_compatibility.CommitValue()) return FALSE;
    const int tab = m_tabs.GetCurSel();
    // Reveal the appropriate tab before validation so errors focus a visible field.
    ShowTab(0);
    if (!CommitMenu()) return FALSE;
    ShowTab(1);
    if (!m_java.CommitPaths()) return FALSE;
    ShowTab(tab);
    m_settings.menus = !!m_menus;
    m_settings.Save();
    return __super::OnApply();
}
