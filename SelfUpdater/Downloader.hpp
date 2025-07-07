#pragma once

#include <Windows.h>
#include <Wininet.h>
#include <atlbase.h> // CComPtr
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <urlmon.h>
#include <vector>

#include "Utils.hpp"

#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "urlmon.lib")

namespace selfUpdater::downloader
{

using ProgressCallBack = std::function<void(const uint64_t&, const uint64_t&)>;

class DownloadProgress : public IBindStatusCallback
{
public:
	HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**)
	{
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef(void)
	{
		return 1;
	}
	ULONG STDMETHODCALLTYPE Release(void)
	{
		return 1;
	}
	HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD dwReserved, IBinding* pib)
	{
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE GetPriority(LONG* pnPriority)
	{
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE OnLowResource(DWORD reserved)
	{
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT hresult, LPCWSTR szError)
	{
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD* grfBINDF, BINDINFO* pbindinfo)
	{
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD grfBSCF, DWORD dwSize, FORMATETC* pformatetc, STGMEDIUM* pstgmed)
	{
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID riid, IUnknown* punk)
	{
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
	{
		// std::cout << std::format("Progress: {}/{} - {}", ulProgress, ulProgressMax, ulStatusCode) << std::endl;

		if (ulStatusCode == BINDSTATUS_DOWNLOADINGDATA || ulStatusCode == BINDSTATUS_BEGINDOWNLOADDATA || ulStatusCode == BINDSTATUS_ENDDOWNLOADDATA)
		{
			for (ProgressCallBack& callback : m_callbacks)
				callback(static_cast<uint64_t>(ulProgress), static_cast<uint64_t>(ulProgressMax));
		}

		return S_OK;
	}

	void AddCallback(ProgressCallBack callback)
	{
		if (callback != nullptr)
			m_callbacks.push_back(callback);
	}

private:
	std::vector<ProgressCallBack> m_callbacks;
};

class Downloader
{
	inline static const std::wstring USER_AGENT = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:126.0) Gecko/20100101 Firefox/126.0";

	struct ComInit
	{
		HRESULT hr;
		ComInit() :
			hr(::CoInitialize(nullptr))
		{}

		~ComInit()
		{
			if (SUCCEEDED(hr)) ::CoUninitialize();
		}
	};

public:
	using Headers = std::map<std::wstring, std::wstring>;

public:
	static bool DownloadSync(const std::wstring& url, const std::wstring& filePath, ProgressCallBack cb = nullptr)
	{
		return download2File(url, filePath, cb);
	}

	static bool DownloadSync(const std::wstring& url, std::vector<uint8_t>& data, Headers* pHeaders = nullptr, ProgressCallBack cb = nullptr)
	{
		return download2Mem(url, data, pHeaders, cb);
	}

private:
	Downloader() = default;

	static bool download2File(const std::wstring& url, const std::wstring& filePath, ProgressCallBack cb)
	{
		DownloadProgress progress;
		progress.AddCallback(cb);

		DeleteUrlCacheEntry(url.c_str());

		HRESULT hr = URLDownloadToFile(NULL, url.c_str(), filePath.c_str(), 0, static_cast<IBindStatusCallback*>(&progress));

		if (FAILED(hr))
		{
			std::wcerr << std::format(L"Download of {} failed: rc={}", url, hr) << std::endl;
			return false;
		}

		return true;
	}

	static bool download2Mem(const std::wstring& url, std::vector<uint8_t>& data, Headers* pHeaders, ProgressCallBack cb)
	{
		data.clear();
		ComInit init;

		DownloadProgress progress;
		progress.AddCallback(cb);

		DeleteUrlCacheEntry(url.c_str());

		CComPtr<IStream> pStream;

		HRESULT hr = URLOpenBlockingStreamW(nullptr, url.c_str(), &pStream, 0, static_cast<IBindStatusCallback*>(&progress));
		if (FAILED(hr))
		{
			std::cerr << std::format("ERROR: Could not connect. HRESULT: {:#x}", hr) << std::endl;
			return false;
		}

		uint8_t buffer[4096];

		do
		{
			DWORD bytesRead = 0;

			hr = pStream->Read(buffer, sizeof(buffer), &bytesRead);

			if (bytesRead > 0)
				data.insert(data.end(), buffer, buffer + bytesRead);

		} while (SUCCEEDED(hr) && hr != S_FALSE);

		if (FAILED(hr))
		{
			std::cerr << std::format("ERROR: Download failed. HRESULT: {:#x}", hr) << std::endl;
			return false;
		}

		if (pHeaders != nullptr)
			getResponseHeaders(url, pHeaders);

		return true;
	}

	static ULONG getResourceSize(const std::wstring& url)
	{
	}

	static bool getResponseHeaders(const std::wstring& url, Headers* pHeaders)
	{
		pHeaders->clear();

		HINTERNET hInternet = InternetOpen(USER_AGENT.c_str(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

		if (hInternet == NULL)
		{
			std::cerr << "Failed to open internet" << std::endl;
			return false;
		}

		HINTERNET hUrl = InternetOpenUrl(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
		if (hUrl == NULL)
		{
			std::cerr << "Failed to open URL" << std::endl;
			InternetCloseHandle(hInternet);
			return false;
		}

		// Query for the response header
		DWORD dwSize = 0;
		HttpQueryInfo(hUrl, HTTP_QUERY_RAW_HEADERS_CRLF, NULL, &dwSize, NULL);

		std::wstring responseHeader;

		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			responseHeader = std::wstring(dwSize, L'\0');
			HttpQueryInfo(hUrl, HTTP_QUERY_RAW_HEADERS_CRLF, &responseHeader[0], &dwSize, NULL);
		}

		// Query for the content length
		dwSize = 0;
		HttpQueryInfo(hUrl, HTTP_QUERY_CONTENT_LENGTH, NULL, &dwSize, NULL);

		std::wstring contentLength;

		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			contentLength = std::wstring(dwSize, L'\0');
			HttpQueryInfo(hUrl, HTTP_QUERY_CONTENT_LENGTH, &contentLength[0], &dwSize, NULL);
		}

		InternetCloseHandle(hUrl);
		InternetCloseHandle(hInternet);

		// Parse the headers
		size_t pos = 0;
		while (pos < responseHeader.size())
		{
			size_t end = responseHeader.find(L"\r\n", pos);
			if (end == std::wstring::npos)
				break;

			std::wstring line = responseHeader.substr(pos, end - pos);
			pos               = end + 2;

			size_t colon = line.find(L":");
			if (colon == std::wstring::npos)
				continue;

			std::wstring key   = line.substr(0, colon);
			std::wstring value = line.substr(colon + 1);
			utils::ltrim(value);

			(*pHeaders)[key] = value;
		}

		return true;
	}
};

inline bool Download(const std::wstring& url, const std::wstring& filePath, ProgressCallBack cb = nullptr)
{
	return Downloader::DownloadSync(url, filePath, cb);
}

inline bool Download(const std::wstring& url, std::vector<uint8_t>& data, Downloader::Headers* pHeaders = nullptr, ProgressCallBack cb = nullptr)
{
	return Downloader::DownloadSync(url, data, pHeaders, cb);
}

} // namespace selfUpdater::downloader