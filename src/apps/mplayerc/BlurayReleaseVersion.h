// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <string_view>

namespace BlurayRelease {

using Version = std::array<unsigned, 4>;

// Accept only exact major.minor.patch-bluray.revision release tags.
inline bool Parse(std::wstring_view text, Version& result)
{
	Version parsed{};
	for (size_t i = 0; i < parsed.size(); ++i) {
		if (text.empty() || text.front() < L'0' || text.front() > L'9') {
			return false;
		}
		size_t digits = 0;
		while (!text.empty() && text.front() >= L'0' && text.front() <= L'9') {
			if (++digits > 5) {
				return false;
			}
			parsed[i] = parsed[i] * 10 + (text.front() - L'0');
			text.remove_prefix(1);
		}
		const std::wstring_view separator = i == 2 ? L"-bluray." : (i < 3 ? L"." : L"");
		if (text.substr(0, separator.size()) != separator) {
			return false;
		}
		text.remove_prefix(separator.size());
	}
	if (!text.empty() || parsed[3] == 0) {
		return false;
	}
	result = parsed;
	return true;
}

} // namespace BlurayRelease
