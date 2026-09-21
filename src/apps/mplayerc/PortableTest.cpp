// SPDX-License-Identifier: GPL-3.0-or-later
#include "stdafx.h"
#include "PortableTest.h"
#if MPCBE_PORTABLE_TEST
#include "PortableProfileImport.h"
#include <ShlObj.h>
#include <KnownFolders.h>
#pragma comment(lib, "crypt32.lib")

namespace {
using namespace PortableProfile;

class CPortableSetup final : public CDialog {
    fs::path m_destination, m_detectedIni, m_manualIni, m_roamingDirectory;
    bool m_registryAvailable = false;
    UINT m_selection = IDC_PT_DEFAULTS;
    CToolTipCtrl m_sourceTooltip;

    fs::path SelectedFile() const {
        return m_selection == IDC_PT_AUTO_INI ? m_detectedIni : m_manualIni;
    }

    bool SelectionAvailable() const {
        switch (m_selection) {
        case IDC_PT_REGISTRY: return m_registryAvailable;
        case IDC_PT_AUTO_INI: return !m_detectedIni.empty();
        case IDC_PT_MANUAL_INI: return !m_manualIni.empty();
        case IDC_PT_DEFAULTS: return true;
        default: return false;
        }
    }

    void SetSourceText(UINT id, const CString& text) {
        SetDlgItemTextW(id, text);
        if (m_sourceTooltip.GetSafeHwnd()) {
            m_sourceTooltip.UpdateTipText(text, GetDlgItem(id));
        }
    }

    void UpdateControls() {
        for (const UINT id : {IDC_PT_REGISTRY, IDC_PT_AUTO_INI, IDC_PT_MANUAL_INI, IDC_PT_DEFAULTS}) {
            CheckDlgButton(id, m_selection == id ? BST_CHECKED : BST_UNCHECKED);
        }
        GetDlgItem(IDC_PT_REGISTRY)->EnableWindow(m_registryAvailable);
        GetDlgItem(IDC_PT_AUTO_INI)->EnableWindow(!m_detectedIni.empty());
        SetDlgItemTextW(IDC_PT_REGISTRY_STATUS, ResStr(m_registryAvailable ? IDS_PT_FOUND : IDS_PT_NOT_FOUND));
        SetDlgItemTextW(IDC_PT_INI_STATUS, ResStr(!m_detectedIni.empty() ? IDS_PT_FOUND : IDS_PT_NOT_FOUND));
        CString detected;
        if (!m_detectedIni.empty()) {
            detected = m_detectedIni.c_str();
        } else if (!m_roamingDirectory.empty()) {
            detected.Format(ResStr(IDS_PT_SEARCH_FOLDER), m_roamingDirectory.c_str());
        }
        SetSourceText(IDC_PT_AUTO_PATH, detected);
        SetSourceText(IDC_PT_MANUAL_PATH, m_manualIni.empty() ? ResStr(IDS_PT_FILE_NOT_SELECTED) : CString(m_manualIni.c_str()));
        GetDlgItem(IDOK)->EnableWindow(SelectionAvailable());
    }
public:
    explicit CPortableSetup(const fs::path& destination)
        : CDialog(IDD_PORTABLE_SETUP), m_destination(destination) {}
protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        if (m_sourceTooltip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX)) {
            m_sourceTooltip.SetMaxTipWidth(600);
            m_sourceTooltip.AddTool(GetDlgItem(IDC_PT_AUTO_PATH), L" ");
            m_sourceTooltip.AddTool(GetDlgItem(IDC_PT_MANUAL_PATH), L" ");
            m_sourceTooltip.Activate(TRUE);
        }
        PWSTR roaming = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&roaming))) {
            m_roamingDirectory = fs::path(roaming)/L"MPC-BE";
            // Prefer this build's INI name, then the other architecture's name.
            for (const auto& filename : {m_destination.filename(), fs::path(L"mpc-be64.ini"), fs::path(L"mpc-be.ini")}) {
                const fs::path candidate = m_roamingDirectory/filename;
                std::error_code ec;
                if (fs::is_regular_file(candidate, ec) && !SameFile(candidate, m_destination)) {
                    m_detectedIni = candidate;
                    break;
                }
            }
        }
        CoTaskMemFree(roaming);
        Key key;
        m_registryAvailable = RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\MPC-BE\\Settings",0,KEY_READ,&key.h) == ERROR_SUCCESS;
        m_selection = !m_detectedIni.empty() ? IDC_PT_AUTO_INI : m_registryAvailable ? IDC_PT_REGISTRY : IDC_PT_DEFAULTS;
        UpdateControls();
        return TRUE;
    }
    BOOL PreTranslateMessage(MSG* message) override {
        if (m_sourceTooltip.GetSafeHwnd()) {
            m_sourceTooltip.RelayEvent(message);
        }
        return CDialog::PreTranslateMessage(message);
    }
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override {
        const UINT id = LOWORD(wParam);
        if ((id == IDC_PT_REGISTRY || id == IDC_PT_AUTO_INI || id == IDC_PT_MANUAL_INI || id == IDC_PT_DEFAULTS) && HIWORD(wParam) == BN_CLICKED) {
            m_selection = id;
            UpdateControls();
            return TRUE;
        }
        if (LOWORD(wParam) == IDC_PT_BROWSE && HIWORD(wParam) == BN_CLICKED) {
            const fs::path initial = m_manualIni.empty() ? m_detectedIni : m_manualIni;
            const CString filter = ResStr(IDS_PT_INI_FILTER);
            CFileDialog picker(TRUE, L"ini", initial.empty() ? nullptr : initial.c_str(),
                OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR, filter, this);
            CString title = ResStr(IDS_PT_SELECT_FILE);
            picker.m_ofn.lpstrTitle = title;
            if (picker.DoModal() == IDOK) {
                const fs::path candidate(picker.GetPathName().GetString());
                std::error_code ec;
                if (_wcsicmp(candidate.extension().c_str(), L".ini") != 0 || !fs::is_regular_file(candidate,ec) || SameFile(candidate,m_destination)) {
                    AfxMessageBox(IDS_PT_BAD_SOURCE,MB_ICONEXCLAMATION);
                } else {
                    m_manualIni = candidate;
                    m_selection = IDC_PT_MANUAL_INI;
                    UpdateControls();
                }
            }
            return TRUE;
        }
        return CDialog::OnCommand(wParam,lParam);
    }
    void OnOK() override {
        const bool imported = m_selection != IDC_PT_DEFAULTS;
        const bool registry = m_selection == IDC_PT_REGISTRY;
        if (!SelectionAvailable()) {
            AfxMessageBox(IDS_PT_BAD_SOURCE,MB_ICONEXCLAMATION);
            return;
        }
        try {
            Snapshot snapshot;
            const fs::path source = SelectedFile();
            const fs::path sourceDirectory = registry ? m_roamingDirectory : source.parent_path();
            if (imported) {
                if (!registry) Require(!SameFile(source,m_destination));
                snapshot = registry ? ReadRegistry(HKEY_CURRENT_USER,L"Software\\MPC-BE") : ReadIni(source);
            }
            Prepare(snapshot,sourceDirectory,imported);
            // Complete asset reads/copies before committing the ready marker.
            if (imported) CopyShaders(sourceDirectory,m_destination.parent_path());
            Commit(m_destination,snapshot);
            CDialog::OnOK();
        } catch (const std::exception&) {
            AfxMessageBox(IDS_PT_IMPORT_FAILED,MB_ICONEXCLAMATION);
        }
    }
};
}
#endif

bool InitializePortableTest()
{
#if MPCBE_PORTABLE_TEST
    auto& profile = AfxGetProfile();
    CString language;
    profile.ReadString(L"Settings",L"Language",language);
    CMPlayerCApp::SetLanguage(language.IsEmpty() ? CMPlayerCApp::GetDefLanguage() : CMPlayerCApp::GetLanguageIndex(language),false);
    SetCurrentProcessExplicitAppUserModelID(L"MPC-BE.BluRay.PortableTest");
    try {
        const std::filesystem::path destination(profile.GetIniPath().GetString());
        // Serialize first-start setup for this folder, including /new launches.
        PortableProfile::File startupLock(CreateFileW((destination.wstring()+L".startup-lock").c_str(),
            GENERIC_READ | GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,nullptr));
        PortableProfile::Require(startupLock.h != INVALID_HANDLE_VALUE);
        if (PortableProfile::NeedsSetup(destination)) {
            CPortableSetup setup(destination);
            if (setup.DoModal() != IDOK) return false;
            profile.ReloadIni();
        }
        PortableProfile::CheckWritable(destination);
    } catch (const std::exception&) {
        AfxMessageBox(IDS_PT_WRITE_FAILED,MB_ICONERROR);
        return false;
    }
#endif
    return true;
}
