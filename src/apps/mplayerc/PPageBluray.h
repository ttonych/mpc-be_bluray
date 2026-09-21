// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PPageBase.h"
#include "BlurayDialogs.h"

class CPPageBluray : public CPPageBase {
    DECLARE_DYNAMIC(CPPageBluray)
    CComboBox m_region, m_country, m_menuLanguage, m_audioLanguage, m_subtitleLanguage, m_age, m_output;
    CTabCtrl m_tabs;
    std::vector<HWND> m_menuControls;
    std::vector<std::pair<CStringW, CStringW>> m_countries, m_languages;
    BOOL m_menus = FALSE;
    BluraySettings m_settings;
    CBlurayJavaDlg m_java;
    CBlurayAdvancedDlg m_compatibility;
    void FillCombo(CComboBox& combo, const std::vector<std::pair<CStringW, CStringW>>& choices, const CStringW& value);
    bool ReadCode(CComboBox& combo, const std::vector<std::pair<CStringW, CStringW>>& choices, int length, CStringW& code);
    bool CommitMenu();
    void ShowTab(int tab);
public:
    enum { IDD = IDD_PPAGEBLURAY };
    CPPageBluray();
protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    BOOL OnApply() override;
    BOOL OnKillActive() override;
    afx_msg void OnCodeEdited(UINT id);
    afx_msg void OnRegionChanged();
    afx_msg void OnTabChanging(NMHDR*, LRESULT*);
    afx_msg void OnTabChanged(NMHDR*, LRESULT*);
    DECLARE_MESSAGE_MAP()
};
