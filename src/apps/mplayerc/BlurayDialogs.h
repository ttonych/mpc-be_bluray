// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BluraySettings.h"
#include "BlurayCatalog.h"
#include <functional>

namespace BlurayUi {
using Choices = std::vector<std::pair<CStringW, BlurayAdvanced::Value>>;
CStringW DisplayValue(int field, BlurayAdvanced::Value value);
Choices ValueChoices(int field);
void FillValue(CComboBox& combo, int field, BlurayAdvanced::Value value);
bool ReadValue(CComboBox& combo, int field, BlurayAdvanced::Value& value);
}

// Modeless children share the property page's draft. Only Apply writes it.
class CBlurayAdvancedDlg : public CDialog {
    CListCtrl m_list;
    CComboBox m_value, m_choice;
    BluraySettings& m_settings;
    std::function<void()> m_modified;
    int m_selected = -1, m_editing = -1;
    bool m_loading = false, m_committing = false, m_useChoice = false;
    CStringW m_filter;
    void Populate();
    void ShowDescription();
    void BeginEdit();
    void CancelEdit();
    CComboBox& Editor() { return m_useChoice ? m_choice : m_value; }
public:
    CBlurayAdvancedDlg(BluraySettings& settings, std::function<void()> modified)
        : CDialog(IDD_BD_ADVANCED), m_settings(settings), m_modified(modified) {}
    bool CommitValue();
protected:
    void DoDataExchange(CDataExchange* dx) override;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG* message) override;
    void OnOK() override {}
    void OnCancel() override {}
    afx_msg void OnChanging(NMHDR*, LRESULT*);
    afx_msg void OnChanged(NMHDR*, LRESULT*);
    afx_msg void OnEdit(NMHDR*, LRESULT*);
    afx_msg void OnDefault();
    afx_msg void OnFilter();
    afx_msg void OnDetails();
    afx_msg void OnValueChosen();
    afx_msg void OnEditorBlur();
    afx_msg LRESULT OnBeginEdit(WPARAM, LPARAM);
    DECLARE_MESSAGE_MAP()
};

class CBlurayJavaDlg : public CDialog {
    BluraySettings& m_settings;
    std::function<void()> m_modified;
    CComboBox m_java, m_persistent, m_cache;
    bool m_loading = true;
    CStringW PathValue(CComboBox& combo, UINT defaultLabel) const;
    void UpdateJavaStatus();
public:
    CBlurayJavaDlg(BluraySettings& settings, std::function<void()> modified)
        : CDialog(IDD_BD_JAVA), m_settings(settings), m_modified(modified) {}
    bool CommitPaths();
protected:
    void DoDataExchange(CDataExchange* dx) override;
    BOOL OnInitDialog() override;
    void OnOK() override {}
    void OnCancel() override {}
    afx_msg void OnBrowse(UINT id);
    afx_msg void OnPathChanged(UINT id);
    afx_msg void OnPersistent();
    afx_msg void OnDiscs();
    DECLARE_MESSAGE_MAP()
};

class CBlurayDiscsDlg : public CDialog {
    CListCtrl m_list;
    CToolTipCtrl m_tooltip;
    std::filesystem::path m_data, m_legacy, m_persistentRoot, m_cacheRoot;
    std::vector<BlurayCatalog::Entry> m_entries;
    std::wstring m_selectedId;
    bool m_loading = false;
    void Refresh(bool reload = true);
    int Selected() const;
    void ShowSelection();
    bool DataFolder(const BlurayCatalog::Entry& entry, std::filesystem::path& path) const;
public:
    CBlurayDiscsDlg(const std::filesystem::path& data, const std::filesystem::path& persistentRoot,
        const std::filesystem::path& cacheRoot, CWnd* parent)
        : CDialog(IDD_BD_DISCS, parent), m_data(data),
        m_legacy(persistentRoot.empty() ? data / L"persistent" : persistentRoot),
        m_persistentRoot(persistentRoot), m_cacheRoot(cacheRoot) {}
protected:
    void DoDataExchange(CDataExchange* dx) override;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG* message) override;
    afx_msg void OnChanged(NMHDR*, LRESULT*);
    afx_msg void OnBeginRename(NMHDR*, LRESULT*);
    afx_msg void OnEndRename(NMHDR*, LRESULT*);
    afx_msg void OnOpen();
    afx_msg void OnRename();
    afx_msg void OnRefresh();
    afx_msg void OnFilter();
    afx_msg void OnReset();
    afx_msg LRESULT OnRefreshList(WPARAM, LPARAM);
    DECLARE_MESSAGE_MAP()
};
