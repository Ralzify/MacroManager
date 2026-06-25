#pragma once
#include <string>
#include <functional>

enum class UpdateState
{
    Idle,
    Checking,
    UpToDate,
    UpdateAvailable,
    Downloading,
    ReadyToInstall,
    Launching,
    Failed
};

struct UpdateInfo
{
    std::string LatestVersion;
    std::string TagName;
    std::string ReleaseUrl;
    std::string ReleaseNotes;
    std::string AssetDownloadUrl;
    std::string AssetName;
    long long AssetSizeBytes = 0;
};

class Updater
{
public:
    static Updater& Get();

    void CheckForUpdatesAsync();

    void Poll();

    void BeginDownloadAndInstallAsync();

    void RemindLater();

    bool IsVersionSkipped(const std::string& version) const;

    UpdateState State() const { return CurrentState; }
    const UpdateInfo& PendingUpdate() const { return Pending; }
    std::string LastError() const { return Error; }
    float DownloadProgress() const { return Progress; }

    static std::string ScratchDir();

private:
    Updater() = default;

    UpdateState CurrentState = UpdateState::Idle;
    UpdateInfo  Pending;
    std::string Error;
    float       Progress = -1.0f;
    std::string DownloadedInstallerPath;

    struct PendingResult
    {
        bool HasCheckResult = false;
        bool CheckSucceeded = false;
        bool UpdateFound = false;
        UpdateInfo Info;
        std::string CheckError;

        bool HasDownloadUpdate = false;
        bool DownloadDone = false;
        bool DownloadSucceeded = false;
        float DownloadProgress = -1.0f;
        std::string DownloadedPath;
        std::string DownloadError;
    };

    void RunCheckThread();
    void RunDownloadThread();
    void LaunchInstallerAndCleanUp(const std::string& installerPath);

    void* ResultMutex = nullptr;
    PendingResult Shared;
    bool CheckInFlight = false;
    bool DownloadInFlight = false;
};
