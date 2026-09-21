// SPDX-License-Identifier: GPL-3.0-or-later
#include "stdafx.h"
#include "BlurayDialogs.h"
#include "BlurayMenu.h"

namespace BlurayUi {
bool Contains(const CStringW& text, const CStringW& filter) {
    if (filter.IsEmpty()) return true;
    // CString::MakeLower follows the CRT locale, which may only lowercase ASCII.
    // Ordinal Unicode comparison also handles Cyrillic independently of that locale.
    for (int i = 0; i <= text.GetLength() - filter.GetLength(); ++i) {
        if (CompareStringOrdinal(text.GetString() + i, filter.GetLength(),
            filter.GetString(), filter.GetLength(), TRUE) == CSTR_EQUAL) return true;
    }
    return false;
}
Choices ValueChoices(int field) {
    using namespace BlurayAdvanced;
    Choices choices{{ResStr(IDS_BD_DEFAULT_VALUE + field), {}}};
    auto add = [&](const CStringW& label, uint32_t value) { choices.push_back({label, {true, value}}); };
    switch (field) {
    case Age:
        for (auto age : {4u, 6u, 12u, 16u, 18u}) {
            CStringW label; label.Format(L"%u", age); add(label, age);
        }
        add(ResStr(IDS_BD_UNLIMITED), 255); break;
    case Profile: {
        const unsigned values[] = {0x100, 0x10110, 0x30200, 0x80200, 0x130240, 0x300, 0x310};
        for (int i = 0; i < 7; ++i) add(ResStr(IDS_BD_PROFILE_CHOICE + i), values[i]);
        break;
    }
    case Restrictions: {
        const unsigned values[] = {0, 5, 10, 20};
        for (int i = 0; i < 4; ++i) add(ResStr(IDS_BD_UO_CHOICE + i), values[i]);
        break;
    }
    case Output: add(L"2D", 0); add(L"3D", 1); break;
    case Decode: add(ResStr(IDS_BD_OFF), 0); add(ResStr(IDS_BD_ON), 1); break;
    case Audio: add(ResStr(IDS_BD_AUDIO_STEREO), 0x5555); add(ResStr(IDS_BD_AUDIO_SURROUND), 0xaaaa); break;
    default: break;
    }
    return choices;
}
CStringW DisplayValue(int field, BlurayAdvanced::Value value) {
    for (const auto& choice : ValueChoices(field)) {
        if (value.enabled == choice.second.enabled && (!value.enabled || value.number == choice.second.number)) return choice.first;
    }
    CStringW text; text.Format(BlurayAdvanced::Specs[field].hex ? L"0x%08X" : L"%u", value.number);
    return text;
}
void FillValue(CComboBox& combo, int field, BlurayAdvanced::Value value) {
    combo.ResetContent();
    for (const auto& choice : ValueChoices(field)) combo.AddString(choice.first);
    const CStringW text = DisplayValue(field, value);
    const int index = combo.FindStringExact(-1, text);
    if (index >= 0) combo.SetCurSel(index); else combo.SetWindowTextW(text);
    combo.LimitText(160);
}
bool ReadValue(CComboBox& combo, int field, BlurayAdvanced::Value& value) {
    CStringW text;
    if (combo.GetCurSel() >= 0) combo.GetLBText(combo.GetCurSel(), text);
    else combo.GetWindowTextW(text);
    text.Trim();
    for (const auto& choice : ValueChoices(field)) if (text == choice.first) { value = choice.second; return true; }
    uint32_t number;
    if (!BlurayAdvanced::Parse(text.GetString(), number) || !BlurayAdvanced::Valid(field, number)) {
        AfxMessageBox(ResStr(IDS_BD_BAD_NUMBER), MB_ICONEXCLAMATION); combo.SetFocus(); return false;
    }
    value = {true, number}; return true;
}
}

BEGIN_MESSAGE_MAP(CBlurayAdvancedDlg, CDialog)
    ON_NOTIFY(LVN_ITEMCHANGING, IDC_BD_ADV_LIST, OnChanging)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_BD_ADV_LIST, OnChanged)
    ON_NOTIFY(NM_DBLCLK, IDC_BD_ADV_LIST, OnEdit)
    ON_BN_CLICKED(IDC_BD_ADV_DEFAULT, OnDefault)
    ON_EN_CHANGE(IDC_BD_FILTER, OnFilter)
    ON_BN_CLICKED(IDC_BD_ADV_DETAILS, OnDetails)
    ON_CBN_SELENDOK(IDC_BD_ADV_VALUE, OnValueChosen)
    ON_CBN_SELENDOK(IDC_BD_ADV_CHOICE, OnValueChosen)
    ON_CBN_KILLFOCUS(IDC_BD_ADV_VALUE, OnEditorBlur)
    ON_CBN_KILLFOCUS(IDC_BD_ADV_CHOICE, OnEditorBlur)
    ON_MESSAGE(WM_APP + 2, OnBeginEdit)
END_MESSAGE_MAP()

void CBlurayAdvancedDlg::DoDataExchange(CDataExchange* dx) {
    __super::DoDataExchange(dx);
    DDX_Control(dx, IDC_BD_ADV_LIST, m_list);
    DDX_Control(dx, IDC_BD_ADV_VALUE, m_value);
    DDX_Control(dx, IDC_BD_ADV_CHOICE, m_choice);
}
BOOL CBlurayAdvancedDlg::OnInitDialog() {
    __super::OnInitDialog();
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    CRect rect; m_list.GetClientRect(rect);
    const int width = rect.Width() - GetSystemMetrics(SM_CXVSCROLL) - 4;
    m_list.InsertColumn(0, ResStr(IDS_BD_PARAMETER), LVCFMT_LEFT, width / 2);
    m_list.InsertColumn(1, ResStr(IDS_BD_VALUE), LVCFMT_LEFT, width - width / 2);
    m_value.ShowWindow(SW_HIDE); m_choice.ShowWindow(SW_HIDE);
    Populate(); return TRUE;
}
void CBlurayAdvancedDlg::Populate() {
    const int previous = m_selected;
    m_loading = true; m_selected = -1;
    m_list.SetRedraw(FALSE); m_list.DeleteAllItems();
    int select = 0;
    CStringW filter(m_filter); filter.Trim();
    for (int field = 0; field < BlurayAdvanced::Count; ++field) {
        if (field == BlurayAdvanced::Age || field == BlurayAdvanced::Output) continue;
        CStringW name = ResStr(IDS_BD_ADV_NAME + field);
        if (!BlurayUi::Contains(name, filter)) continue;
        const int item = m_list.InsertItem(m_list.GetItemCount(), name);
        m_list.SetItemData(item, field);
        m_list.SetItemText(item, 1, BlurayUi::DisplayValue(field, m_settings.advanced[field]));
        if (field == previous) select = item;
    }
    m_loading = false;
    if (m_list.GetItemCount()) m_list.SetItemState(select, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    m_list.SetRedraw(TRUE); m_list.Invalidate(); ShowDescription();
}
void CBlurayAdvancedDlg::ShowDescription() {
    SetDlgItemTextW(IDC_BD_ADV_DESCRIPTION, m_selected >= 0 ? ResStr(IDS_BD_SHORT_DESC + m_selected) : CStringW());
    const bool details = IsDlgButtonChecked(IDC_BD_ADV_DETAILS) == BST_CHECKED;
    SetDlgItemTextW(IDC_BD_ADV_TECHNICAL, details && m_selected >= 0 ? ResStr(IDS_BD_ADV_DESC + m_selected) : CStringW());
    GetDlgItem(IDC_BD_ADV_TECHNICAL)->ShowWindow(details ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_BD_ADV_DEFAULT)->EnableWindow(m_selected >= 0 && m_settings.advanced[m_selected].enabled);
}
void CBlurayAdvancedDlg::BeginEdit() {
    if (m_selected < 0 || !CommitValue()) return;
    const int row = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (row < 0) return;
    m_list.EnsureVisible(row, FALSE);
    CRect rect; m_list.GetSubItemRect(row, 1, LVIR_BOUNDS, rect);
    m_list.ClientToScreen(rect); ScreenToClient(rect);
    m_useChoice = m_selected == BlurayAdvanced::Profile || m_selected == BlurayAdvanced::Restrictions || m_selected == BlurayAdvanced::Decode;
    m_loading = true;
    auto& editor = Editor();
    BlurayUi::FillValue(editor, m_selected, m_settings.advanced[m_selected]);
    CRect drop(0, 0, 0, 110); MapDialogRect(drop);
    editor.SetWindowPos(&wndTop, rect.left, rect.top - 1, rect.Width(), drop.bottom, SWP_SHOWWINDOW);
    editor.SetDroppedWidth(rect.Width());
    m_editing = m_selected; m_loading = false;
    editor.SetFocus();
    if (!m_useChoice) editor.SetEditSel(0, -1);
}
void CBlurayAdvancedDlg::CancelEdit() {
    m_editing = -1;
    m_value.ShowWindow(SW_HIDE); m_choice.ShowWindow(SW_HIDE);
}
bool CBlurayAdvancedDlg::CommitValue() {
    if (m_editing < 0 || m_loading || m_committing) return true;
    m_committing = true;
    BlurayAdvanced::Value value;
    if (!BlurayUi::ReadValue(Editor(), m_editing, value)) { m_committing = false; return false; }
    const auto old = m_settings.advanced[m_editing];
    m_settings.advanced[m_editing] = value;
    for (int item = 0; item < m_list.GetItemCount(); ++item)
        if (int(m_list.GetItemData(item)) == m_editing) m_list.SetItemText(item, 1, BlurayUi::DisplayValue(m_editing, value));
    CancelEdit(); m_committing = false; ShowDescription();
    if (old.enabled != value.enabled || (value.enabled && old.number != value.number)) m_modified();
    return true;
}
BOOL CBlurayAdvancedDlg::PreTranslateMessage(MSG* message) {
    if (message->message == WM_KEYDOWN) {
        if (m_editing >= 0 && message->wParam == VK_ESCAPE) { CancelEdit(); m_list.SetFocus(); return TRUE; }
        if (message->wParam == VK_RETURN || message->wParam == VK_F2) {
            if (m_editing >= 0) { if (CommitValue()) m_list.SetFocus(); return TRUE; }
            if (::GetFocus() == m_list.m_hWnd) { BeginEdit(); return TRUE; }
        }
    }
    // Keep a floating editor attached to its row when the list scrolls/resizes.
    if (m_editing >= 0 && (message->message == WM_MOUSEWHEEL || message->message == WM_LBUTTONDOWN)
        && message->hwnd != Editor().m_hWnd && !::IsChild(Editor().m_hWnd, message->hwnd)) {
        if (!CommitValue()) return TRUE;
    }
    return __super::PreTranslateMessage(message);
}
void CBlurayAdvancedDlg::OnChanging(NMHDR* header, LRESULT* result) {
    const auto* n = reinterpret_cast<NMLISTVIEW*>(header); *result = 0;
    if (!m_loading && (n->uChanged & LVIF_STATE) && (n->uOldState & LVIS_SELECTED)
        && !(n->uNewState & LVIS_SELECTED) && !CommitValue()) *result = TRUE;
}
void CBlurayAdvancedDlg::OnChanged(NMHDR* header, LRESULT* result) {
    const auto* n = reinterpret_cast<NMLISTVIEW*>(header); *result = 0;
    if (!m_loading && (n->uChanged & LVIF_STATE) && (n->uNewState & LVIS_SELECTED)) {
        m_selected = int(m_list.GetItemData(n->iItem)); ShowDescription();
    }
}
void CBlurayAdvancedDlg::OnEdit(NMHDR*, LRESULT* result) {
    *result = 0;
    // The list restores focus after NM_DBLCLK. Open after it finishes, otherwise
    // that focus change immediately closes the newly created cell editor.
    PostMessage(WM_APP + 2);
}
LRESULT CBlurayAdvancedDlg::OnBeginEdit(WPARAM, LPARAM) { BeginEdit(); return 0; }
void CBlurayAdvancedDlg::OnDefault() {
    if (m_selected < 0) return;
    CancelEdit(); m_settings.advanced[m_selected] = {}; Populate(); m_modified();
}
void CBlurayAdvancedDlg::OnFilter() {
    if (m_loading) return;
    if (!CommitValue()) {
        m_loading = true; SetDlgItemTextW(IDC_BD_FILTER, m_filter); m_loading = false; return;
    }
    GetDlgItemTextW(IDC_BD_FILTER, m_filter); Populate();
}
void CBlurayAdvancedDlg::OnDetails() {
    if (!CommitValue()) return;
    const bool details = IsDlgButtonChecked(IDC_BD_ADV_DETAILS) == BST_CHECKED;
    // All twelve rows fit normally. Technical detail uses space below a shorter table.
    CRect list(4, 22, 270, details ? 123 : 166); MapDialogRect(list); m_list.MoveWindow(list);
    CRect description(4, details ? 128 : 169, 270, details ? 157 : 192); MapDialogRect(description);
    GetDlgItem(IDC_BD_ADV_DESCRIPTION)->MoveWindow(description);
    CRect technical(4, 162, 270, 192); MapDialogRect(technical);
    GetDlgItem(IDC_BD_ADV_TECHNICAL)->MoveWindow(technical);
    ShowDescription();
}
void CBlurayAdvancedDlg::OnValueChosen() {
    if (m_loading || m_editing < 0) return;
    if (CommitValue()) m_list.SetFocus();
}
void CBlurayAdvancedDlg::OnEditorBlur() {
    if (m_editing < 0 || m_committing || Editor().GetDroppedState()) return;
    CommitValue();
}

BEGIN_MESSAGE_MAP(CBlurayJavaDlg, CDialog)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_BD_BROWSE_JAVA, IDC_BD_BROWSE_CACHE, OnBrowse)
    ON_CONTROL_RANGE(CBN_EDITCHANGE, IDC_BD_JAVA_HOME, IDC_BD_CACHE_ROOT, OnPathChanged)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_BD_JAVA_HOME, IDC_BD_CACHE_ROOT, OnPathChanged)
    ON_BN_CLICKED(IDC_BD_PERSISTENT, OnPersistent)
    ON_BN_CLICKED(IDC_BD_DISCS, OnDiscs)
END_MESSAGE_MAP()
void CBlurayJavaDlg::DoDataExchange(CDataExchange* dx) {
    __super::DoDataExchange(dx);
    DDX_Control(dx, IDC_BD_JAVA_HOME, m_java);
    DDX_Control(dx, IDC_BD_PERSIST_ROOT, m_persistent);
    DDX_Control(dx, IDC_BD_CACHE_ROOT, m_cache);
}
BOOL CBlurayJavaDlg::OnInitDialog() {
    __super::OnInitDialog();
    auto fill = [](CComboBox& combo, UINT label, CStringW value) {
        value.Trim();
        combo.AddString(ResStr(label)); combo.LimitText(32760);
        if (value.IsEmpty()) combo.SetCurSel(0); else combo.SetWindowTextW(value);
    };
    fill(m_java, IDS_BD_AUTO_SEARCH, m_settings.javaHome);
    fill(m_persistent, IDS_BD_PLAYER_FOLDER, m_settings.persistentRoot);
    fill(m_cache, IDS_BD_PLAYER_FOLDER, m_settings.cacheRoot);
    CheckDlgButton(IDC_BD_PERSISTENT, m_settings.persistent ? BST_CHECKED : BST_UNCHECKED);
    m_loading = false; UpdateJavaStatus(); return TRUE;
}
CStringW CBlurayJavaDlg::PathValue(CComboBox& combo, UINT label) const {
    CStringW text;
    if (combo.GetCurSel() >= 0) combo.GetLBText(combo.GetCurSel(), text); else combo.GetWindowTextW(text);
    text.Trim(); if (text == ResStr(label)) text.Empty(); return text;
}
void CBlurayJavaDlg::UpdateJavaStatus() {
    const CStringW path = PathValue(m_java, IDS_BD_AUTO_SEARCH);
    UINT status = IDS_BD_JAVA_AUTO;
    if (!path.IsEmpty()) status = PathFileExistsW(path + L"\\bin\\server\\jvm.dll") || PathFileExistsW(path + L"\\jre\\bin\\server\\jvm.dll")
        ? IDS_BD_JAVA_FOUND : IDS_BD_JAVA_NOT_FOUND;
    SetDlgItemTextW(IDC_BD_JAVA_STATUS, ResStr(status));
}
void CBlurayJavaDlg::OnPathChanged(UINT) { if (!m_loading) { UpdateJavaStatus(); m_modified(); } }
void CBlurayJavaDlg::OnPersistent() { m_settings.persistent = IsDlgButtonChecked(IDC_BD_PERSISTENT) == BST_CHECKED; m_modified(); }
void CBlurayJavaDlg::OnBrowse(UINT id) {
    CComboBox* combos[] = {&m_java, &m_persistent, &m_cache};
    auto& combo = *combos[id - IDC_BD_BROWSE_JAVA];
    const auto path = PathValue(combo, id == IDC_BD_BROWSE_JAVA ? IDS_BD_AUTO_SEARCH : IDS_BD_PLAYER_FOLDER);
    CFolderPickerDialog picker(path.IsEmpty() ? nullptr : path.GetString(), OFN_PATHMUSTEXIST, this);
    if (picker.DoModal() == IDOK) { combo.SetCurSel(-1); combo.SetWindowTextW(picker.GetPathName()); OnPathChanged(0); }
}
bool CBlurayJavaDlg::CommitPaths() {
    CStringW values[] = {PathValue(m_java, IDS_BD_AUTO_SEARCH), PathValue(m_persistent, IDS_BD_PLAYER_FOLDER), PathValue(m_cache, IDS_BD_PLAYER_FOLDER)};
    CComboBox* combos[] = {&m_java, &m_persistent, &m_cache};
    for (int i = 0; i < 3; ++i) {
        const auto& value = values[i];
        if (!value.IsEmpty() && (!std::filesystem::path(value.GetString()).is_absolute()
            || value.Find(L'"') >= 0 || value.Find(L'\n') >= 0 || value.Find(L'\r') >= 0)) {
            AfxMessageBox(ResStr(IDS_BD_BAD_PATH), MB_ICONEXCLAMATION); combos[i]->SetFocus(); return false;
        }
    }
    if (!values[0].IsEmpty() && !PathFileExistsW(values[0] + L"\\bin\\server\\jvm.dll") && !PathFileExistsW(values[0] + L"\\jre\\bin\\server\\jvm.dll")) {
        AfxMessageBox(ResStr(IDS_BD_BAD_JAVA), MB_ICONEXCLAMATION); m_java.SetFocus(); return false;
    }
    m_settings.javaHome = values[0]; m_settings.persistentRoot = values[1]; m_settings.cacheRoot = values[2]; return true;
}
void CBlurayJavaDlg::OnDiscs() {
    // Catalogue operations concern saved settings, not unapplied folder edits.
    BluraySettings active; active.Load();
    const std::filesystem::path data(CBlurayMenu::DataDirectory().GetString());
    CBlurayDiscsDlg dialog(data, std::filesystem::path(active.persistentRoot.GetString()),
        std::filesystem::path(active.cacheRoot.GetString()), this);
    dialog.DoModal();
}

BEGIN_MESSAGE_MAP(CBlurayDiscsDlg, CDialog)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_BD_DISC_LIST, OnChanged)
    ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_BD_DISC_LIST, OnBeginRename)
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_BD_DISC_LIST, OnEndRename)
    ON_BN_CLICKED(IDC_BD_DISC_OPEN, OnOpen)
    ON_BN_CLICKED(IDC_BD_DISC_RENAME, OnRename)
    ON_BN_CLICKED(IDC_BD_DISC_REFRESH, OnRefresh)
    ON_BN_CLICKED(IDC_BD_RESET_DISC, OnReset)
    ON_EN_CHANGE(IDC_BD_FILTER, OnFilter)
    ON_MESSAGE(WM_APP + 1, OnRefreshList)
END_MESSAGE_MAP()
void CBlurayDiscsDlg::DoDataExchange(CDataExchange* dx) { __super::DoDataExchange(dx); DDX_Control(dx, IDC_BD_DISC_LIST, m_list); }
int CBlurayDiscsDlg::Selected() const {
    const int row = m_list.GetNextItem(-1, LVNI_SELECTED);
    return row < 0 ? -1 : int(m_list.GetItemData(row));
}
BOOL CBlurayDiscsDlg::OnInitDialog() {
    __super::OnInitDialog();
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    CRect rect; m_list.GetClientRect(rect);
    m_list.InsertColumn(0, ResStr(IDS_BD_DISC_TITLE), LVCFMT_LEFT, rect.Width() * 2 / 3);
    m_list.InsertColumn(1, ResStr(IDS_BD_DISC_LAST), LVCFMT_LEFT, rect.Width() / 3 - GetSystemMetrics(SM_CXVSCROLL) - 4);
    m_tooltip.Create(this); m_tooltip.SetMaxTipWidth(600); m_tooltip.Activate(TRUE);
    m_tooltip.AddTool(GetDlgItem(IDC_BD_DISC_OPEN), L" ");
    Refresh(); return TRUE;
}
static CStringW LocalDiscTime(const std::wstring& value) {
    SYSTEMTIME utc{}, local{};
    if (value.size() != 23 || value.substr(19) != L" UTC"
        || swscanf_s(value.c_str(), L"%hu-%hu-%hu %hu:%hu:%hu", &utc.wYear, &utc.wMonth, &utc.wDay,
            &utc.wHour, &utc.wMinute, &utc.wSecond) != 6
        || !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return value.c_str();
    wchar_t date[128]{}, time[128]{};
    if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr, date, _countof(date), nullptr)
        || !GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, time, _countof(time))) return value.c_str();
    return CStringW(date) + L" " + time;
}
void CBlurayDiscsDlg::Refresh(bool reload) {
    m_loading = true; m_list.SetRedraw(FALSE); m_list.DeleteAllItems();
    if (reload) m_entries = BlurayCatalog::List(m_data, m_legacy);
    CStringW filter; GetDlgItemTextW(IDC_BD_FILTER, filter); filter.Trim();
    int selection = 0;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const auto& e = m_entries[i]; CStringW label(e.Label().c_str());
        if (!BlurayUi::Contains(label, filter)) continue;
        if (e.legacy) label += L" \u2014 " + ResStr(IDS_BD_LEGACY);
        const int row = m_list.InsertItem(m_list.GetItemCount(), label);
        m_list.SetItemData(row, i); m_list.SetItemText(row, 1, LocalDiscTime(e.seen));
        if (e.id == m_selectedId) selection = row;
    }
    m_loading = false;
    if (m_list.GetItemCount()) m_list.SetItemState(selection, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    m_list.SetRedraw(TRUE); m_list.Invalidate(); ShowSelection();
    CStringW count;
    count.Format(ResStr(filter.IsEmpty() ? IDS_BD_DISC_COUNT : IDS_BD_DISC_FILTER_COUNT), m_list.GetItemCount(), int(m_entries.size()));
    SetDlgItemTextW(IDC_BD_DISC_COUNT, count);
}
bool CBlurayDiscsDlg::DataFolder(const BlurayCatalog::Entry& entry, std::filesystem::path& path) const {
    if (entry.legacy) { path = entry.folder; return !path.empty(); }
    // After Reset use the new generation, not the stale catalogue path. Before
    // any reset the recorded folder is where this disc was actually last opened.
    std::filesystem::path persistent, cache;
    if (!BlurayDiscStorage::Resolve(m_data, entry.id, persistent, cache)) return false;
    if (persistent == m_data / L"persistent") { path = entry.folder; return !path.empty(); }
    return BlurayDiscStorage::Resolve(m_data, entry.id, path, cache, m_persistentRoot, m_cacheRoot);
}
void CBlurayDiscsDlg::ShowSelection() {
    const int index = Selected();
    const bool selected = index >= 0 && size_t(index) < m_entries.size();
    GetDlgItem(IDC_BD_DISC_RENAME)->EnableWindow(selected);
    GetDlgItem(IDC_BD_RESET_DISC)->EnableWindow(selected && !m_entries[index].legacy && BlurayDiscStorage::ValidKey(m_entries[index].id));
    std::filesystem::path folder;
    const bool available = selected && DataFolder(m_entries[index], folder) && PathIsDirectoryW(folder.c_str());
    GetDlgItem(IDC_BD_DISC_OPEN)->EnableWindow(available);
    CStringW tip(folder.c_str());
    if (selected) {
        m_selectedId = m_entries[index].id;
        if (m_entries[index].legacy || std::count_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
            std::filesystem::path other; return DataFolder(e, other) && other == folder;
        }) > 1) tip = ResStr(IDS_BD_SHARED_FOLDER) + L"\r\n" + tip;
    }
    m_tooltip.UpdateTipText(tip.IsEmpty() ? L" " : tip.GetString(), GetDlgItem(IDC_BD_DISC_OPEN));
}
void CBlurayDiscsDlg::OnChanged(NMHDR*, LRESULT* result) { *result = 0; if (!m_loading) ShowSelection(); }
void CBlurayDiscsDlg::OnOpen() {
    const int index = Selected(); if (index < 0) return;
    std::filesystem::path path;
    if (!DataFolder(m_entries[index], path) || !PathIsDirectoryW(path.c_str())
        || (INT_PTR)ShellExecuteW(m_hWnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL) <= 32)
        AfxMessageBox(ResStr(IDS_BD_STORAGE_ERROR), MB_ICONERROR);
}
void CBlurayDiscsDlg::OnRename() {
    const int row = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (row >= 0) { m_list.SetFocus(); m_list.EditLabel(row); }
}
void CBlurayDiscsDlg::OnBeginRename(NMHDR* header, LRESULT* result) {
    const auto* info = reinterpret_cast<NMLVDISPINFO*>(header); *result = 0;
    const auto& entry = m_entries[m_list.GetItemData(info->item.iItem)];
    if (auto* edit = m_list.GetEditControl()) { edit->SetLimitText(250); edit->SetWindowTextW(entry.Label().c_str()); }
}
void CBlurayDiscsDlg::OnEndRename(NMHDR* header, LRESULT* result) {
    const auto* info = reinterpret_cast<NMLVDISPINFO*>(header); *result = FALSE;
    if (!info->item.pszText) return;
    const int index = int(m_list.GetItemData(info->item.iItem));
    auto entry = BlurayCatalog::Read(m_data, m_entries[index].id);
    if (entry.name.empty()) entry = m_entries[index];
    CStringW alias(info->item.pszText); alias.Trim(); entry.alias = alias.GetString();
    if (!BlurayCatalog::Write(m_data, entry)) { AfxMessageBox(ResStr(IDS_BD_STORAGE_ERROR), MB_ICONERROR); return; }
    m_selectedId = entry.id; m_entries[index] = entry;
    PostMessage(WM_APP + 1); // Refilter only after the native label editor closes.
}
void CBlurayDiscsDlg::OnReset() {
    const int index = Selected(); if (index < 0) return;
    const auto entry = m_entries[index];
    if (entry.legacy || !BlurayDiscStorage::ValidKey(entry.id)) return;
    CStringW question; question.Format(ResStr(IDS_BD_RESET_DATA_CONFIRM), entry.Label().c_str());
    if (AfxMessageBox(question, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    if (!BlurayDiscStorage::Reset(m_data, entry.id)) { AfxMessageBox(ResStr(IDS_BD_STORAGE_ERROR), MB_ICONERROR); return; }
    std::filesystem::path folder, cache;
    if (BlurayDiscStorage::Resolve(m_data, entry.id, folder, cache, m_persistentRoot, m_cacheRoot)) {
        // Custom roots' generation directories normally appear on disc open.
        // Create the persistent directory now so Open data folder works at once.
        SHCreateDirectoryExW(m_hWnd, folder.c_str(), nullptr);
    }
    ShowSelection(); AfxMessageBox(ResStr(IDS_BD_RESET_DATA_DONE), MB_ICONINFORMATION);
}
void CBlurayDiscsDlg::OnRefresh() { Refresh(); }
void CBlurayDiscsDlg::OnFilter() { if (!m_loading) Refresh(false); }
LRESULT CBlurayDiscsDlg::OnRefreshList(WPARAM, LPARAM) { Refresh(false); return 0; }
BOOL CBlurayDiscsDlg::PreTranslateMessage(MSG* message) {
    m_tooltip.RelayEvent(message);
    if (message->message == WM_KEYDOWN) {
        if (auto* edit = m_list.GetEditControl()) {
            if (message->wParam == VK_RETURN) { m_list.SetFocus(); return TRUE; }
            if (message->wParam == VK_ESCAPE) { m_list.SendMessage(LVM_CANCELEDITLABEL); return TRUE; }
        } else if (message->wParam == VK_F2 && ::GetFocus() == m_list.m_hWnd) { OnRename(); return TRUE; }
    }
    return __super::PreTranslateMessage(message);
}
