#pragma once

#include <Windows.h>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <map>

#include "Downloader.hpp"
#include "Utils.hpp"
#include "Version.hpp"

#ifndef SU_BASE_URL
#define SU_BASE_URL L""
#endif

#ifndef SU_VERSION_FILENAME
#define SU_VERSION_FILENAME L"versions.txt"
#endif

// Based on: https://www.codeproject.com/Articles/1205548/An-efficient-way-for-automatic-updating

class SelfUpdater
{
	static inline const std::wstring TEMP_PREFIX = L"_U_";

	using VerMap  = std::map<std::string, selfUpdater::version::ResVersion>;
	using VerMapW = std::map<std::wstring, selfUpdater::version::ResVersion>;

public:
	static inline std::wstring s_baseUrl         = SU_BASE_URL;
	static inline std::wstring s_versionFilename = SU_VERSION_FILENAME;
	static inline HWND s_mainHWnd                = nullptr;

	using UpdateCallBack = std::function<void(void)>;

	enum class UpdateType
	{
		Window,
		Console,
		Custom
	};

	enum class UpdateMode
	{
		Blocking,
		NonBlocking
	};

public:
	static SelfUpdater& GetInstance()
	{
		static SelfUpdater instance;
		return instance;
	}

	static void Setup()
	{
		GetInstance();
	}

	static void SetMainHWnd(HWND hWnd)
	{
		s_mainHWnd = hWnd;
	}

	static void SetBaseUrl(const std::wstring& baseUrl)
	{
		s_baseUrl = baseUrl;
	}

	static void SetVersionFilename(const std::wstring& filename)
	{
		s_versionFilename = filename;
	}

	static bool CheckForUpdates(const UpdateType& type = UpdateType::Console, const UpdateMode& mode = UpdateMode::NonBlocking, const UpdateCallBack& cb = nullptr)
	{
		return GetInstance().checkForUpdates(type, mode, cb);
	}

	static bool WaitUntilDone()
	{
		return GetInstance().waitUntilDone();
	}

	static bool DoUpdate()
	{
		return GetInstance().doUpdate();
	}

	static void CleanUp()
	{
		GetInstance().cleanUp();
	}

	static void UpdateAvailableWindow()
	{
		// Show a message box to the user, asking if they want to update
		if (MessageBox(s_mainHWnd, L"An update is available. Do you want to update now?", L"Update Available", MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			std::cout << "Starting self updating ..." << std::endl;
			DoUpdate();
		}
		else
		{
			std::cout << "User declined update" << std::endl;
			CleanUp();
		}
	}

	static void UpdateAvailableConsole()
	{
		std::cout << "An update is available. Do you want to update now? (Y/n)" << std::endl;
		std::string input;
		std::getline(std::cin, input);
		if (input.empty() || input[0] == 'y' || input[0] == 'Y')
		{
			std::cout << "Starting self updating ..." << std::endl;
			DoUpdate();
		}
		else
		{
			std::cout << "User declined update" << std::endl;
			CleanUp();
		}
	}

private:
	SelfUpdater()
	{
		init();
	}

	SelfUpdater(const SelfUpdater&)            = delete;
	SelfUpdater& operator=(const SelfUpdater&) = delete;
	SelfUpdater(SelfUpdater&&)                 = delete;
	SelfUpdater& operator=(SelfUpdater&&)      = delete;

	bool checkForUpdates(const UpdateType& type = UpdateType::Console, const UpdateMode& mode = UpdateMode::NonBlocking, const UpdateCallBack& cb = nullptr)
	{
		switch (type)
		{
			case UpdateType::Window:
				m_callback = UpdateAvailableWindow;
				break;
			case UpdateType::Console:
				m_callback = UpdateAvailableConsole;
				break;
			case UpdateType::Custom:
				if (cb == nullptr)
				{
					std::cerr << "Custom update callback is null" << std::endl;
					return false;
				}
				m_callback = cb;
				break;
			default:
				std::cerr << "Unknown update type" << std::endl;
				break;
		}

		// std::exit() called from within std::async does not terminate the main process (at least using MSVC).
		// Therefore, we have to build part of it from scratch.
		// The thread is detached to prevent it from crashing the application when exit is called within it.
		// The promise is used to get the result of the thread and to allow for a blocking wait on it, e.g., when waiting until the update evaluation is done.

		std::promise<bool> resultPromise;
		m_updateThrdRes = resultPromise.get_future();

		std::thread([this, promise = std::move(resultPromise)]() mutable {
			try
			{
				bool result = this->checkForUpdatesThrd();
				promise.set_value(result);
			}
			catch (...)
			{
				promise.set_exception(std::current_exception());
			}
		}).detach();

		if (mode == UpdateMode::Blocking)
			return waitUntilDone();

		return true;
	}

	bool waitUntilDone()
	{
		bool res = false;
		try
		{
			// Blocks until result is set or thread exits
			res = m_updateThrdRes.get();
		}
		catch (const std::exception& e)
		{
			std::cerr << "Exception from update thread: " << e.what() << "\n";
		}

		return res;
	}

	bool doUpdate()
	{
		std::wstring url = std::format(L"{}/{}", s_baseUrl, m_exeName);
		m_tempExePath    = std::format(L"{}\\{}{}", std::filesystem::temp_directory_path().wstring(), TEMP_PREFIX, m_exeName);

		bool res = selfUpdater::downloader::Download(url, m_tempExePath);
		if (!res)
		{
			std::wcerr << L"Failed to download the new version" << std::endl;
			return false;
		}

		// Set the new exe path
		m_newExePath = std::format(L"{}\\{}", m_exePath, std::filesystem::path(m_tempExePath).filename().wstring());

		std::wcout << std::format(L"New version downloaded to: {}", m_tempExePath) << std::endl;
		std::wcout << std::format(L"Copying the new version to: {}", m_newExePath) << std::endl;

		// Copy the temp version to the new path
		if (!std::filesystem::copy_file(m_tempExePath, m_newExePath, std::filesystem::copy_options::overwrite_existing))
		{
			std::wcerr << L"Failed to copy the temp version to the new path" << std::endl;
			return false;
		}

		if (exec(m_newExePath))
		{
			std::wcout << L"Exiting old instance" << std::endl;
			exit(0);
		}
		else
		{
			std::wcerr << L"Couldn't start the new version" << std::endl;
			return false;
		}
	}

	void cleanUp()
	{
		if (!m_tempExePath.empty() && std::filesystem::exists(m_tempExePath))
			std::filesystem::remove(m_tempExePath);
	}

	bool checkForUpdatesThrd()
	{
		if (m_isTemp)
			return true;

		std::cout << "Checking for updates..." << std::endl;

		std::wstring url = std::format(L"{}/{}", s_baseUrl, s_versionFilename);

		std::vector<uint8_t> versionData;

		bool res = selfUpdater::downloader::Download(url, versionData);
		if (res)
		{
			VerMapW versions = parseVersionFileDataW(versionData);

			selfUpdater::version::ResVersion newVer;

			for (const auto& [name, ver] : versions)
			{
				if (name == m_exeName)
					newVer = ver;
			}

			if (!newVer)
			{
				std::wcerr << std::format(L"Couldn't find the version info for {} in the version file", m_exeName) << std::endl;
				return false;
			}

			if (newVer <= m_version)
			{
				std::cout << "No new version available" << std::endl;
				return false;
			}

			std::wcout << std::format(L"New version available: {} -> {}\n", m_version.ToWString(), newVer.ToWString());

			if (m_callback)
				m_callback();
		}

		return res;
	}

	void init()
	{
		// Get the path to the current executable
		std::filesystem::path p = selfUpdater::utils::GetExecutablePath();

		m_fullExePath = p.wstring();
		m_exePath     = p.parent_path().wstring();
		m_exeName     = p.filename().wstring();

		m_version = selfUpdater::version::GetVersionInfo();
		replaceTempVersion();
	}

	bool replaceTempVersion()
	{
		std::wcout << m_exeName << std::endl;
		// Check if the exe name starts with "_U_"
		if (m_exeName.find(TEMP_PREFIX) == 0)
		{
			std::wstring realName = m_exeName.substr(TEMP_PREFIX.size());
			std::wstring realPath = std::format(L"{}\\{}", m_exePath, realName);
			m_isTemp              = true;
			std::wcout << L"Running from temp version ..." << std::endl;

			// Wait for the old instance to exit
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			// Remove the old version
			try
			{
				for (uint32_t i = 0; i < 5; i++)
				{
					if (std::filesystem::exists(realPath))
					{
						if (!std::filesystem::remove(realPath))
						{
							std::wcerr << L"Failed to remove the old version, sleeping and retrying ..." << std::endl;
							std::this_thread::sleep_for(std::chrono::milliseconds(100));
						}
					}
				}
			}
			catch (const std::exception& e)
			{
				std::wcerr << L"Failed to remove the old version: " << e.what() << std::endl;
				return false;
			}

			// Copy the temp version
			if (!std::filesystem::copy_file(m_fullExePath, realPath))
			{
				std::wcerr << L"Failed to copy the temp version to the real path" << std::endl;
				return false;
			}

			// Start the new version
			if (exec(realPath))
			{
				std::wcout << L"Exiting temp instance" << std::endl;
				exit(0);
			}
			else
			{
				std::wcerr << L"Failed to start the new version" << std::endl;
				return false;
			}
		}

		if (std::filesystem::exists(std::format(L"{}\\_U_{}", m_exePath, m_exeName)))
		{
			std::wcout << L"Running from normal version, but old temp exists, deleting ..." << std::endl;
			std::filesystem::remove(std::format(L"{}\\_U_{}", m_exePath, m_exeName));
		}

		return true;
	}

	static bool exec(const std::wstring& fileName)
	{
		std::wcout << std::format(L"Executing: {} ... ", fileName) << std::flush;

		PROCESS_INFORMATION ProcessInfo;
		STARTUPINFO StartupInfo;

		ZeroMemory(&StartupInfo, sizeof(StartupInfo));
		StartupInfo.cb = sizeof StartupInfo;

		if (CreateProcess(fileName.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &StartupInfo, &ProcessInfo))
		{
			CloseHandle(ProcessInfo.hThread);
			CloseHandle(ProcessInfo.hProcess);
			std::wcout << L"Successful" << std::endl;
			return true;
		}
		else
		{
			std::wcout << L"Failed" << std::endl;
			return false;
		}
	}

	static VerMap parseVersionFileData(const std::vector<uint8_t>& data)
	{
		VerMap versions;
		std::string versionString(data.begin(), data.end());
		std::vector<std::string> lines = selfUpdater::utils::Split(versionString, "\n");
		for (const std::string& l : lines)
		{
			std::vector<std::string> parts = selfUpdater::utils::Split(l, "\t");
			if (parts.size() == 2)
			{
				std::string name = parts[0];
				selfUpdater::version::ResVersion ver(parts[1]);
				if (ver)
					versions[name] = ver;
			}
		}

		return versions;
	}

	static VerMapW parseVersionFileDataW(const std::vector<uint8_t>& data)
	{
		VerMapW versions;
		const VerMap vv = parseVersionFileData(data);
		for (const auto& [name, ver] : vv)
		{
			std::wstring wName = selfUpdater::utils::s2ws(name);
			versions[wName]    = ver;
		}

		return versions;
	}

private:
	selfUpdater::version::ResVersion m_version = {};

	std::wstring m_exeName     = L"";
	std::wstring m_exePath     = L"";
	std::wstring m_fullExePath = L"";
	std::wstring m_newExePath  = L"";
	std::wstring m_tempExePath = L"";
	UpdateCallBack m_callback  = nullptr;

	bool m_isTemp = false;

	std::future<bool> m_updateThrdRes = {};
};
