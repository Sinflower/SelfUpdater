#pragma once

#include <Windows.h>
#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "Utils.hpp"

#pragma comment(lib, "version.lib")

namespace selfUpdater::version
{
class ResVersion
{
public:
	static inline bool s_msFormat = false; // true = major.minor.build.revision, false = major.minor.revision.build

public:
	ResVersion() = default;
	ResVersion(const uint16_t& major, const uint16_t& minor, const uint16_t& revision = 0, const uint16_t& build = 0) :
		m_major(major), m_minor(minor), m_revision(revision), m_build(build)
	{
		m_valid = true;
	}

	ResVersion(const uint32_t& ms, const uint32_t& ls)
	{
		m_major    = HIWORD(ms);
		m_minor    = LOWORD(ms);
		m_build    = (s_msFormat ? HIWORD(ls) : LOWORD(ls));
		m_revision = (s_msFormat ? LOWORD(ls) : HIWORD(ls));

		m_valid = true;
	}

	explicit ResVersion(const std::string& verStr)
	{
		std::vector<std::string> parts = utils::Split(verStr, '.');
		if (parts.size() == 4)
		{
			m_major    = static_cast<uint16_t>(std::stoi(parts[0]));
			m_minor    = static_cast<uint16_t>(std::stoi(parts[1]));
			m_revision = static_cast<uint16_t>(std::stoi(parts[2]));
			m_build    = static_cast<uint16_t>(std::stoi(parts[3]));
			m_valid    = true;
		}
		else if (parts.size() == 3)
		{
			m_major    = static_cast<uint16_t>(std::stoi(parts[0]));
			m_minor    = static_cast<uint16_t>(std::stoi(parts[1]));
			m_revision = static_cast<uint16_t>(std::stoi(parts[2]));
			m_build    = 0;
			m_valid    = true;
		}
		else
		{
			m_valid = false;
			return;
		}

		// Because in the default Microsoft format version string is defined as major.minor.build.revision revision and build have to be swapped
		if (s_msFormat)
			std::swap(m_build, m_revision);
	}

	operator bool() const
	{
		return m_valid;
	}

	const bool IsValid() const
	{
		return m_valid;
	}

	void SetMajor(const uint16_t& major)
	{
		m_major = major;
	}

	void SetMinor(const uint16_t& minor)
	{
		m_minor = minor;
	}

	void SetRevision(const uint16_t& revision)
	{
		m_revision = revision;
	}

	void SetBuild(const uint16_t& build)
	{
		m_build = build;
	}

	const uint16_t& GetMajor() const
	{
		return m_major;
	}

	const uint16_t& GetMinor() const
	{
		return m_minor;
	}

	const uint16_t& GetRevision() const
	{
		return m_revision;
	}

	const uint16_t& GetBuild() const
	{
		return m_build;
	}

	std::wstring ToWString(const uint32_t lvl = 3) const
	{
		return utils::s2ws(toString(lvl));
	}

	std::string ToString(const uint32_t lvl = 3) const
	{
		return toString(lvl);
	}

	bool operator>(const ResVersion& other) const
	{
		if (m_major > other.m_major)
			return true;
		if (m_major < other.m_major)
			return false;

		if (m_minor > other.m_minor)
			return true;
		if (m_minor < other.m_minor)
			return false;

		if (s_msFormat)
		{
			if (m_build > other.m_build)
				return true;
			if (m_build < other.m_build)
				return false;
			if (m_revision > other.m_revision)
				return true;
			if (m_revision < other.m_revision)
				return false;
		}
		else
		{
			if (m_revision > other.m_revision)
				return true;
			if (m_revision < other.m_revision)
				return false;
			if (m_build > other.m_build)
				return true;
			if (m_build < other.m_build)
				return false;
		}

		return false;
	}

	bool operator<(const ResVersion& other) const
	{
		if (m_major < other.m_major)
			return true;
		if (m_major > other.m_major)
			return false;

		if (m_minor < other.m_minor)
			return true;
		if (m_minor > other.m_minor)
			return false;

		if (s_msFormat)
		{
			if (m_build < other.m_build)
				return true;
			if (m_build > other.m_build)
				return false;
			if (m_revision < other.m_revision)
				return true;
			if (m_revision > other.m_revision)
				return false;
		}
		else
		{
			if (m_revision < other.m_revision)
				return true;
			if (m_revision > other.m_revision)
				return false;
			if (m_build < other.m_build)
				return true;
			if (m_build > other.m_build)
				return false;
		}

		return false;
	}

	bool operator==(const ResVersion& other) const
	{
		if (s_msFormat)
			return m_major == other.m_major && m_minor == other.m_minor && m_build == other.m_build && m_revision == other.m_revision;
		else
			return m_major == other.m_major && m_minor == other.m_minor && m_revision == other.m_revision && m_build == other.m_build;
	}

	bool operator!=(const ResVersion& other) const
	{
		return !(*this == other);
	}

	bool operator<=(const ResVersion& other) const
	{
		return *this < other || *this == other;
	}

	bool operator>=(const ResVersion& other) const
	{
		return *this > other || *this == other;
	}

	///////////////////////////
	/// String Operators Start
	///////////////////////////

	std::string operator+(const std::string& str) const
	{
		return str + ToString(4);
	}

	std::string operator+(const char* pStr) const
	{
		return std::string(pStr) + ToString(4);
	}

	std::wstring operator+(const std::wstring& str) const
	{
		return str + ToWString(4);
	}

	std::wstring operator+(const wchar_t* pStr) const
	{
		return std::wstring(pStr) + ToWString(4);
	}

	friend std::string operator+(const std::string& str, const ResVersion& version)
	{
		return str + version.ToString(4);
	}

	friend std::string operator+(const char* pStr, const ResVersion& version)
	{
		return std::string(pStr) + version.ToString(4);
	}

	friend std::wstring operator+(const std::wstring& str, const ResVersion& version)
	{
		return str + version.ToWString(4);
	}

	friend std::wstring operator+(const wchar_t* pStr, const ResVersion& version)
	{
		return std::wstring(pStr) + version.ToWString(4);
	}

	friend std::wostream& operator<<(std::wostream& os, const ResVersion& version)
	{
		os << version.ToWString(4);
		return os;
	}

	friend std::ostream& operator<<(std::ostream& os, const ResVersion& version)
	{
		os << version.ToString(4);
		return os;
	}

	///////////////////////////
	/// String Operators End
	///////////////////////////

	static ResVersion GetVersionInfo()
	{
		TCHAR szPath[MAX_PATH];
		GetModuleFileName(NULL, szPath, MAX_PATH);
		return GetVersionInfo(szPath);
	}

	static ResVersion GetVersionInfo(const std::wstring& exe)
	{
		ResVersion version;
		if (!loadVersionInfo(exe, version))
			std::wcerr << std::format(L"[GetVersionInfo] Couldn't load version info for: {}", exe) << std::endl;

		return version;
	}

private:
	std::string toString(const uint32_t lvl = 3) const
	{
		if (lvl < 1 || lvl > 4)
			throw std::invalid_argument("lvl must be between 1 and 4");

		switch (lvl)
		{
			case 1:
				return std::format("{}", m_major);
			case 2:
				return std::format("{}.{}", m_major, m_minor);
			case 3:
				return std::format("{}.{}.{}", m_major, m_minor, (s_msFormat ? m_build : m_revision));
			case 4:
			{
				if (s_msFormat)
					return std::format("{}.{}.{}.{}", m_major, m_minor, m_build, m_revision);
				else
					return std::format("{}.{}.{} Build #{}", m_major, m_minor, m_revision, m_build);
			}
		}

		return toString();
	}

	static bool loadVersionInfo(const std::wstring& exe, ResVersion& version)
	{
		DWORD verHandle = 0;
		UINT size       = 0;
		LPBYTE lpBuffer = NULL;
		DWORD verSize   = GetFileVersionInfoSize(exe.c_str(), &verHandle);
		bool ret        = false;

		std::memset(&version, 0, sizeof(ResVersion));

		if (verSize != 0)
		{
			LPSTR pVerData = new char[verSize];

			if (GetFileVersionInfo(exe.c_str(), verHandle, verSize, pVerData))
			{
				if (VerQueryValue(pVerData, L"\\", (VOID FAR * FAR*)&lpBuffer, &size))
				{
					if (size)
					{
						VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
						if (verInfo->dwSignature == 0xfeef04bd)
						{
							version = ResVersion(static_cast<uint32_t>(verInfo->dwFileVersionMS), static_cast<uint32_t>(verInfo->dwFileVersionLS));
							ret     = true;
						}
					}
				}
			}
			delete[] pVerData;
		}

		return ret;
	}

private:
	uint16_t m_major    = 0;
	uint16_t m_minor    = 0;
	uint16_t m_revision = 0;
	uint16_t m_build    = 0;

	bool m_valid = false;
};

inline ResVersion GetVersionInfo()
{
	return ResVersion::GetVersionInfo();
}

} // namespace selfUpdater::version