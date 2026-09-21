// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/apps/mplayerc/BlurayReleaseVersion.h"
#include <cassert>
#include <iostream>

int main()
{
	using namespace BlurayRelease;
	Version value{};
	assert(Parse(L"1.9.1-bluray.1", value));
	assert((value == Version{1, 9, 1, 1}));
	for (auto bad : {L"1.9.1", L"1.9.1.2", L"1.9.1-bluray.0", L"v1.9.1-bluray.1",
		L"1.9.1-bluray.2-extra", L"1.9.1-bluray.-1", L"1.9.1-bluray.100000",
		L" 1.9.1-bluray.2", L"1.9.1-bluray.2 ", L"1.9.1-bluray.", L""}) {
		const auto before = value;
		assert(!Parse(bad, value));
		assert(value == before);
	}
	assert(Parse(L"1.9.1-bluray.2", value));
	assert((value > Version{1, 9, 1, 1}));
	assert((Version{1, 9, 2, 1} > value));
	assert((Version{1, 10, 0, 1} > Version{1, 9, 99, 99}));
	std::cout << "Release tag parsing and version ordering: passed\n";
}
