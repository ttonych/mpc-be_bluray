/*
 * (C) 2013-2026 see Authors.txt
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include <afxinet.h>
#include "DSUtil/SysVersion.h"
#include "DSUtil/text.h"
#include "DSUtil/HTTPAsync.h"
#include "rapidjsonHelper.h"
#include "UpdateChecker.h"

#include "BlurayVersion.h"

// UpdateChecker

bool UpdateChecker::IsTimeToAutoUpdate(int delay, time_t lastcheck)
{
	return (time(nullptr) >= lastcheck + delay * 24 * 3600);
}

void UpdateChecker::CheckForUpdate(bool autocheck)
{
	CAutoLock lock(&csUpdating);

	if (!bUpdating) {
		bUpdating = true;
		AfxBeginThread(RunCheckForUpdateThread, (LPVOID)autocheck);
	}
}

Update_Status UpdateChecker::CheckNewVersion()
{
	m_UpdateURL.Empty();
	m_UpdateVersion = {};

	Update_Status updatestatus = UPDATER_ERROR_CONNECT;
	CHTTPAsync HTTPAsync;
	if (SUCCEEDED(HTTPAsync.Connect(L"https://api.github.com/repos/ttonych/mpc-be_bluray/releases?per_page=100", http::connectTimeout))) {
		constexpr auto sizeRead = 16 * KILOBYTE;
		CStringA data;
		DWORD dwSizeRead = 0;
		int dataSize = 0;
		while (S_OK == HTTPAsync.Read(reinterpret_cast<PBYTE>(data.GetBuffer(dataSize + sizeRead) + dataSize), sizeRead, dwSizeRead, http::readTimeout)) {
			data.ReleaseBuffer(dataSize + dwSizeRead);
			dataSize = data.GetLength();
			dwSizeRead = 0;
		}
		data.ReleaseBuffer(dataSize);

		if (!data.IsEmpty()) {
			updatestatus = UPDATER_ERROR_DATA;
			rapidjson::Document json;
			if (!json.Parse(data.GetString()).HasParseError() && json.IsArray()) {
				// Include published prereleases: this fork is distributed for testing.
				updatestatus = UPDATER_NO_NEW_VERSION;
				const BlurayRelease::Version current = { MPC_VERSION_MAJOR, MPC_VERSION_MINOR, MPC_VERSION_PATCH, MPCBE_BLURAY_REVISION };
				for (const auto& release : json.GetArray()) {
					if (!release.IsObject()) {
						continue;
					}
					bool draft = true;
					CString tag, url;
					BlurayRelease::Version candidate{};
					if (getJsonValue(release, "draft", draft) && !draft
							&& getJsonValue(release, "tag_name", tag)
							&& BlurayRelease::Parse(std::wstring_view(tag.GetString(), tag.GetLength()), candidate)
							&& candidate > current && candidate > m_UpdateVersion
							&& getJsonValue(release, "html_url", url)
							&& url.Find(L"https://github.com/ttonych/mpc-be_bluray/releases/tag/") == 0) {
						m_UpdateVersion = candidate;
						m_UpdateURL = url;
						updatestatus = UPDATER_NEW_VERSION_IS_AVAILABLE;
					}
				}
			}
		}
	}

	return updatestatus;
}

UINT UpdateChecker::RunCheckForUpdateThread(LPVOID pParam)
{
	bool autocheck = !!pParam;
	Update_Status updateStatus = CheckNewVersion();

	if (!autocheck || updateStatus == UPDATER_NEW_VERSION_IS_AVAILABLE) {
		CStringW text;
		UINT nType = MB_OK;

		switch (updateStatus) {
		case UPDATER_ERROR_CONNECT:
			text.LoadString(IDS_UPDATE_ERROR_CONNECT);
			nType = MB_OK | MB_ICONHAND;
			break;
		case UPDATER_ERROR_DATA:
			text.LoadString(IDS_UPDATE_ERROR_DATA);
			nType = MB_OK | MB_ICONHAND;
			break;
		case UPDATER_NO_NEW_VERSION:
			text.Format(IDS_USING_NEWER_VERSION, MPCBE_BLURAY_VERSION_WSTR);
			nType = MB_OK | MB_ICONINFORMATION;
			break;
		case UPDATER_NEW_VERSION_IS_AVAILABLE: {
			CStringW versionStr;
			versionStr.Format(L"%u.%u.%u-bluray.%u", m_UpdateVersion[0], m_UpdateVersion[1],
				m_UpdateVersion[2], m_UpdateVersion[3]);
			text.Format(IDS_NEW_UPDATE_AVAILABLE, versionStr);
			nType = MB_YESNO | MB_ICONQUESTION;
			break;
		}
		default:
			ASSERT(0);
		}

		if (updateStatus == UPDATER_NEW_VERSION_IS_AVAILABLE) {
			if (IDYES == AfxMessageBox(text, nType)) {
				ShellExecuteW(nullptr, L"open", m_UpdateURL, nullptr, nullptr, SW_SHOWDEFAULT);
			}
		}
		else {
			AfxMessageBox(text, nType);
		}
	}

	if (autocheck && updateStatus >= UPDATER_NO_NEW_VERSION) {
		AfxGetAppSettings().tUpdaterLastCheck = time(nullptr);
	}

	bUpdating = false;

	return 0;
}
