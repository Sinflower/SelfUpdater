#pragma once

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <format>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace selfUpdater::utils
{
// A helper function to set up the console for debugging in GUI applications
inline void SetupConsole()
{
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONIN$", "r", stdin);
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);

	std::cout.clear();
	std::cerr.clear();
	std::cin.clear();

	std::wcout.clear();
	std::wcerr.clear();
	std::wcin.clear();
}

inline std::wstring ToUTF16(const std::string& utf8String)
{
	if (utf8String.empty()) return std::wstring();

	int32_t size = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);

	if (size == 0)
		return std::wstring();

	std::wstring wideStr(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &wideStr[0], size);

	// Remove the null terminator
	wideStr.pop_back();

	return wideStr;
}

inline std::string ToUTF8(const std::wstring& utf16String)
{
	if (utf16String.empty()) return std::string();

	int32_t size = WideCharToMultiByte(CP_UTF8, 0, utf16String.c_str(), -1, nullptr, 0, nullptr, nullptr);

	if (size == 0)
		return std::string();

	std::string utf8Str(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, utf16String.c_str(), -1, &utf8Str[0], size, nullptr, nullptr);

	// Remove the null terminator
	utf8Str.pop_back();

	return utf8Str;
}

#define s2ws ToUTF16
#define ws2s ToUTF8

inline std::wstring GetExecutablePath(HMODULE hModule = nullptr, uint32_t maxAttempts = 5)
{
	// Workaround to properly call numeric_limits::max even when the max macro of Windows.h is defined
	const DWORD maxAllowedSize = (std::numeric_limits<DWORD>::max)();
	const DWORD initialSize    = MAX_PATH;

	// Determine maximum safe attempts to avoid DWORD overflow
	DWORD maxPossibleAttempts = static_cast<DWORD>(std::log2(static_cast<double>(maxAllowedSize) / initialSize));
	if (maxAttempts > maxPossibleAttempts)
		maxAttempts = maxPossibleAttempts;

	DWORD size = initialSize;
	std::vector<wchar_t> buffer;
	std::wstring lastPathAttempt;

	for (uint32_t attempt = 0; attempt < maxAttempts; attempt++)
	{
		buffer.resize(size);
		DWORD length = GetModuleFileNameW(hModule, buffer.data(), size);

		if (length == 0)
		{
			DWORD error = GetLastError();
			throw std::runtime_error("GetModuleFileNameW failed with error code: " + std::to_string(error));
		}

		lastPathAttempt.assign(buffer.begin(), buffer.begin() + length);

		if (length < size - 1)
		{
			// Successfully retrieved the full path
			return lastPathAttempt;
		}

		// Path may have been truncated, try a larger buffer
		size *= 2;
	}

	// All attempts failed — log what we got last
	std::wstringstream ss;
	ss << L"Failed to retrieve full executable path after " << maxAttempts
	   << L" attempts. Last attempt returned possibly truncated path: \"" << lastPathAttempt << L"\"";

	// Convert wstring to narrow string for std::runtime_error
	std::wstring errorMsg(ss.str().begin(), ss.str().end());
	throw std::runtime_error(ws2s(errorMsg));
}

// trim from start (in place)
inline void ltrim(std::string& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

inline void ltrim(std::wstring& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](wchar_t ch) { return !std::iswspace(ch); }));
}

// trim from end (in place)
inline void rtrim(std::string& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

inline void rtrim(std::wstring& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [](wchar_t ch) { return !std::iswspace(ch); }).base(), s.end());
}

inline std::wstring ReplaceAll(const std::wstring& str, const std::wstring& from, const std::wstring& to)
{
	std::wstring result = str;
	size_t startPos     = 0;
	while ((startPos = result.find(from, startPos)) != std::wstring::npos)
	{
		result.replace(startPos, from.length(), to);
		startPos += to.length();
	}

	return result;
}

inline std::vector<std::string> Split(const std::string& str, const std::string& delimiter)
{
	std::vector<std::string> tokens;
	size_t start = 0;
	size_t end   = str.find(delimiter);
	while (end != std::string::npos)
	{
		tokens.push_back(str.substr(start, end - start));
		start = end + delimiter.length();
		end   = str.find(delimiter, start);
	}
	tokens.push_back(str.substr(start, end));
	return tokens;
}

inline std::vector<std::string> Split(const std::string& str, const char& delimiter)
{
	return Split(str, std::string(1, delimiter));
}
} // namespace selfUpdater::utils
