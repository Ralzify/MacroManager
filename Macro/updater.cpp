#include "updater.h"
#include "version.h"
#include "json.hpp"

#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <cctype>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

using json = nlohmann::json;

namespace
{
    const wchar_t* kApiHost = L"api.github.com";
    const char* kReleasesPath = "/repos/Ralzify/MacroManager/releases";
    const wchar_t* kUserAgent = L"MacroManager-Updater/1.0";

    bool LooksLikeInstallerAsset(const std::string& name)
    {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });

        bool isExe = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".exe") == 0;
        bool hasSetup = lower.find("setup") != std::string::npos;
        return isExe && hasSetup;
    }

    struct ParsedTag
    {
        bool IsPre = false;
        std::string Numeric;
    };

    ParsedTag ParseVersionTag(const std::string& tag)
    {
        ParsedTag result;
        std::string rest = tag;

        if (rest.size() >= 3)
        {
            std::string prefix = rest.substr(0, 3);
            std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });

            if (prefix == "pre")
            {
                result.IsPre = true;
                rest = rest.substr(3);
                while (!rest.empty() && (rest[0] == '-' || rest[0] == '_' || rest[0] == '.'))
                    rest = rest.substr(1);
            }
        }

        if (!rest.empty() && (rest[0] == 'v' || rest[0] == 'V'))
            rest = rest.substr(1);

        result.Numeric = rest;
        return result;
    }

    std::vector<int> ParseVersionParts(const std::string& version)
    {
        std::vector<int> parts;
        std::stringstream ss(version);
        std::string segment;

        while (std::getline(ss, segment, '.'))
        {
            try { parts.push_back(std::stoi(segment)); }
            catch (...) { parts.push_back(0); }
        }

        if (parts.empty())
            parts.push_back(0);

        return parts;
    }

    bool IsNewerVersion(const std::string& candidateTag, const std::string& currentTag)
    {
        ParsedTag candidate = ParseVersionTag(candidateTag);
        ParsedTag current = ParseVersionTag(currentTag);

        if (candidate.IsPre != current.IsPre)
            return !candidate.IsPre;

        std::vector<int> a = ParseVersionParts(candidate.Numeric);
        std::vector<int> b = ParseVersionParts(current.Numeric);
        size_t n = (a.size() > b.size()) ? a.size() : b.size();

        for (size_t i = 0; i < n; ++i)
        {
            int va = (i < a.size()) ? a[i] : 0;
            int vb = (i < b.size()) ? b[i] : 0;

            if (va != vb)
                return va > vb;
        }

        return false;
    }

    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), size);
        return out;
    }

    std::string SkippedVersionsFilePath()
    {
        char path[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
        {
            std::string dir = std::string(path) + "\\Macro Manager";
            CreateDirectoryA(dir.c_str(), nullptr);
            return dir + "\\update_prefs.json";
        }
        return "update_prefs.json";
    }
}

namespace
{
    std::string HttpGetText(const std::wstring& host, const std::wstring& path)
    {
        HINTERNET hSession = WinHttpOpen(kUserAgent,
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession)
            throw std::runtime_error("Could not open a network session.");

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Could not connect to GitHub.");
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Could not create a request to GitHub.");
        }

        const wchar_t* headers = L"Accept: application/vnd.github+json\r\n";
        WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
            && WinHttpReceiveResponse(hRequest, nullptr);

        if (!ok)
        {
            DWORD err = GetLastError();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Network request to GitHub failed (error " + std::to_string(err) + ").");
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        std::string body;
        DWORD bytesAvailable = 0;

        do
        {
            bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
                break;

            if (bytesAvailable == 0)
                break;

            std::vector<char> buffer(bytesAvailable);
            DWORD bytesRead = 0;

            if (!WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead))
                break;

            body.append(buffer.data(), bytesRead);
        } while (bytesAvailable > 0);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (statusCode == 404)
            throw std::runtime_error("No releases were found on GitHub for this repository.");

        if (statusCode == 403)
            throw std::runtime_error("GitHub rate-limited this request. Try again later.");

        if (statusCode < 200 || statusCode >= 300)
            throw std::runtime_error("GitHub returned an unexpected response (HTTP " + std::to_string(statusCode) + ").");

        return body;
    }

    void HttpDownloadFile(const std::wstring& fullUrl, const std::string& destPath,
        const std::function<void(float)>& onProgress)
    {
        URL_COMPONENTS urlComp = {};
        urlComp.dwStructSize = sizeof(urlComp);

        wchar_t hostBuf[256] = {};
        wchar_t pathBuf[2048] = {};
        urlComp.lpszHostName = hostBuf;
        urlComp.dwHostNameLength = 256;
        urlComp.lpszUrlPath = pathBuf;
        urlComp.dwUrlPathLength = 2048;

        if (!WinHttpCrackUrl(fullUrl.c_str(), 0, 0, &urlComp))
            throw std::runtime_error("Could not parse the download URL.");

        bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

        HINTERNET hSession = WinHttpOpen(kUserAgent,
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession)
            throw std::runtime_error("Could not open a network session.");

        HINTERNET hConnect = WinHttpConnect(hSession, hostBuf, urlComp.nPort, 0);
        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Could not connect to the download server.");
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", pathBuf,
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            isHttps ? WINHTTP_FLAG_SECURE : 0);

        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Could not create the download request.");
        }

        bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
            && WinHttpReceiveResponse(hRequest, nullptr);

        if (!ok)
        {
            DWORD err = GetLastError();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Download request failed (error " + std::to_string(err) + ").");
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode < 200 || statusCode >= 300)
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Download failed (HTTP " + std::to_string(statusCode) + ").");
        }

        DWORD contentLength = 0;
        DWORD clSize = sizeof(contentLength);
        bool haveLength = WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_CONTENT_LENGTH,
            WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &clSize, WINHTTP_NO_HEADER_INDEX);

        std::ofstream outFile(destPath, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open())
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Could not create the destination file: " + destPath);
        }

        DWORD totalRead = 0;
        DWORD bytesAvailable = 0;

        do
        {
            bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
                break;

            if (bytesAvailable == 0)
                break;

            std::vector<char> buffer(bytesAvailable);
            DWORD bytesRead = 0;

            if (!WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead))
                break;

            outFile.write(buffer.data(), bytesRead);
            totalRead += bytesRead;

            if (onProgress)
                onProgress(haveLength && contentLength > 0 ? (float)totalRead / (float)contentLength : -1.0f);

        } while (bytesAvailable > 0);

        outFile.close();

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (haveLength && contentLength > 0 && totalRead != contentLength)
            throw std::runtime_error("Download was incomplete (connection may have dropped).");
    }
}

Updater& Updater::Get()
{
    static Updater instance;

    if (!instance.ResultMutex)
    {
        auto* cs = new CRITICAL_SECTION();
        InitializeCriticalSection(cs);
        instance.ResultMutex = cs;
    }

    return instance;
}

std::string Updater::ScratchDir()
{
    char path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path)))
    {
        std::string dir = std::string(path) + "\\MacroManager\\Update";
        return dir;
    }
    return "MacroManagerUpdate";
}

bool Updater::IsVersionSkipped(const std::string& version) const
{
    try
    {
        std::ifstream file(SkippedVersionsFilePath());
        if (!file.is_open())
            return false;

        json root;
        file >> root;

        if (root.contains("skippedVersion") && root["skippedVersion"].is_string())
            return root["skippedVersion"].get<std::string>() == version;
    }
    catch (...) {}

    return false;
}

void Updater::RemindLater()
{
    try
    {
        json root;
        root["skippedVersion"] = Pending.LatestVersion;

        std::ofstream file(SkippedVersionsFilePath(), std::ios::trunc);
        if (file.is_open())
            file << root.dump(2);
    }
    catch (...) {}

    CurrentState = UpdateState::Idle;
}

void Updater::CheckForUpdatesAsync()
{
    if (CheckInFlight)
        return;

    CheckInFlight = true;
    CurrentState = UpdateState::Checking;
    Error.clear();

    std::thread([]()
        {
            Updater::Get().RunCheckThread();
        }).detach();
}

void Updater::RunCheckThread()
{
    auto* cs = (CRITICAL_SECTION*)ResultMutex;
    PendingResult result;
    result.HasCheckResult = true;

    try
    {
        std::string body = HttpGetText(kApiHost, Utf8ToWide(kReleasesPath));
        json releases = json::parse(body);

        if (!releases.is_array() || releases.empty())
            throw std::runtime_error("No releases were found on GitHub for this repository.");

        const json* chosen = nullptr;

        for (const auto& rel : releases)
        {
            bool isDraft = rel.value("draft", false);
            if (!isDraft) { chosen = &rel; break; }
        }

        if (!chosen)
            throw std::runtime_error("No published releases were found (only drafts exist).");

        std::string tagName = chosen->value("tag_name", "");
        ParsedTag parsedTag = ParseVersionTag(tagName);

        if (parsedTag.Numeric.empty())
            throw std::runtime_error("The latest release on GitHub has no version tag.");

        result.CheckSucceeded = true;

        if (!IsNewerVersion(tagName, APP_VERSION_STRING))
        {
            result.UpdateFound = false;
        }
        else
        {
            UpdateInfo info;
            info.TagName = tagName;
            info.LatestVersion = parsedTag.IsPre ? ("Pre-" + parsedTag.Numeric) : parsedTag.Numeric;
            info.ReleaseUrl = chosen->value("html_url", "");
            info.ReleaseNotes = chosen->value("body", "");

            if (chosen->contains("assets") && (*chosen)["assets"].is_array())
            {
                for (const auto& asset : (*chosen)["assets"])
                {
                    std::string name = asset.value("name", "");
                    if (LooksLikeInstallerAsset(name))
                    {
                        info.AssetName = name;
                        info.AssetDownloadUrl = asset.value("browser_download_url", "");
                        info.AssetSizeBytes = asset.value("size", 0LL);
                        break;
                    }
                }
            }

            if (info.AssetDownloadUrl.empty())
                throw std::runtime_error("Release " + tagName + " was found, but it has no installer (.exe) attached yet.");

            result.UpdateFound = true;
            result.Info = info;
        }
    }
    catch (const std::exception& e)
    {
        result.CheckSucceeded = false;
        result.CheckError = e.what();
    }
    catch (...)
    {
        result.CheckSucceeded = false;
        result.CheckError = "An unknown error occurred while checking for updates.";
    }

    EnterCriticalSection(cs);
    Shared.HasCheckResult = true;
    Shared.CheckSucceeded = result.CheckSucceeded;
    Shared.UpdateFound = result.UpdateFound;
    Shared.Info = result.Info;
    Shared.CheckError = result.CheckError;
    LeaveCriticalSection(cs);
}

void Updater::BeginDownloadAndInstallAsync()
{
    if (DownloadInFlight || Pending.AssetDownloadUrl.empty())
        return;

    DownloadInFlight = true;
    CurrentState = UpdateState::Downloading;
    Progress = -1.0f;

    std::thread([]()
        {
            Updater::Get().RunDownloadThread();
        }).detach();
}

void Updater::RunDownloadThread()
{
    auto* cs = (CRITICAL_SECTION*)ResultMutex;
    PendingResult result;
    result.HasDownloadUpdate = true;

    std::string url = Pending.AssetDownloadUrl;
    std::string assetName = Pending.AssetName.empty() ? "MacroManagerSetup.exe" : Pending.AssetName;

    try
    {
        std::string dir = ScratchDir();

        std::string parent = dir.substr(0, dir.find_last_of("\\/"));
        CreateDirectoryA(parent.c_str(), nullptr);
        CreateDirectoryA(dir.c_str(), nullptr);

        std::string destPath = dir + "\\" + assetName;

        HttpDownloadFile(Utf8ToWide(url), destPath, [&](float progress)
            {
                EnterCriticalSection(cs);
                Shared.HasDownloadUpdate = true;
                Shared.DownloadDone = false;
                Shared.DownloadProgress = progress;
                LeaveCriticalSection(cs);
            });

        result.DownloadDone = true;
        result.DownloadSucceeded = true;
        result.DownloadedPath = destPath;
        result.DownloadProgress = 1.0f;
    }
    catch (const std::exception& e)
    {
        result.DownloadDone = true;
        result.DownloadSucceeded = false;
        result.DownloadError = e.what();
    }
    catch (...)
    {
        result.DownloadDone = true;
        result.DownloadSucceeded = false;
        result.DownloadError = "An unknown error occurred while downloading the update.";
    }

    EnterCriticalSection(cs);
    Shared.HasDownloadUpdate = true;
    Shared.DownloadDone = result.DownloadDone;
    Shared.DownloadSucceeded = result.DownloadSucceeded;
    Shared.DownloadedPath = result.DownloadedPath;
    Shared.DownloadError = result.DownloadError;
    Shared.DownloadProgress = result.DownloadProgress;
    LeaveCriticalSection(cs);
}

void Updater::Poll()
{
    auto* cs = (CRITICAL_SECTION*)ResultMutex;
    if (!cs) return;

    EnterCriticalSection(cs);

    bool hasCheck = Shared.HasCheckResult;
    bool checkSucceeded = Shared.CheckSucceeded;
    bool updateFound = Shared.UpdateFound;
    UpdateInfo info = Shared.Info;
    std::string checkError = Shared.CheckError;
    Shared.HasCheckResult = false;

    bool hasDownload = Shared.HasDownloadUpdate;
    bool downloadDone = Shared.DownloadDone;
    bool downloadSucceeded = Shared.DownloadSucceeded;
    float downloadProgress = Shared.DownloadProgress;
    std::string downloadedPath = Shared.DownloadedPath;
    std::string downloadError = Shared.DownloadError;
    Shared.HasDownloadUpdate = false;

    LeaveCriticalSection(cs);

    if (hasCheck)
    {
        CheckInFlight = false;

        if (!checkSucceeded)
        {
            CurrentState = UpdateState::Failed;
            Error = checkError;
        }
        else if (!updateFound)
        {
            CurrentState = UpdateState::UpToDate;
        }
        else
        {
            if (IsVersionSkipped(info.LatestVersion))
            {
                CurrentState = UpdateState::UpToDate;
            }
            else
            {
                Pending = info;
                CurrentState = UpdateState::UpdateAvailable;
            }
        }
    }

    if (hasDownload)
    {
        Progress = downloadProgress;

        if (downloadDone)
        {
            DownloadInFlight = false;

            if (downloadSucceeded)
            {
                DownloadedInstallerPath = downloadedPath;
                CurrentState = UpdateState::ReadyToInstall;
                LaunchInstallerAndCleanUp(DownloadedInstallerPath);
                CurrentState = UpdateState::Launching;
            }
            else
            {
                CurrentState = UpdateState::Failed;
                Error = downloadError;
            }
        }
    }
}

void Updater::LaunchInstallerAndCleanUp(const std::string& installerPath)
{
    std::string dir = ScratchDir();
    std::string watcherPath = dir + "\\cleanup.cmd";

    std::ofstream watcher(watcherPath, std::ios::trunc);

    if (watcher.is_open())
    {
        watcher << "@echo off\r\n" << "start /wait \"\" \"" << installerPath << "\"\r\n" << "del /f /q \"" << installerPath << "\"\r\n" << "del /f /q \"%~f0\"\r\n";
        watcher.close();

        std::wstring wideWatcher = Utf8ToWide(watcherPath);

        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = L"cmd.exe";
        std::wstring params = L"/c \"" + wideWatcher + L"\"";
        sei.lpParameters = params.c_str();
        sei.nShow = SW_HIDE;

        ShellExecuteExW(&sei);
        if (sei.hProcess)
            CloseHandle(sei.hProcess);
    }
    else
    {
        std::wstring widePath = Utf8ToWide(installerPath);
        ShellExecuteW(nullptr, L"open", widePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}