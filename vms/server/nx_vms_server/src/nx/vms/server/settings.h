#pragma once

#include <nx/utils/settings.h>
#include <network/multicodec_rtp_reader.h>
#include <network/system_helpers.h>
#include <utils/common/app_info.h>
#include <utils/common/util.h>

namespace nx {
namespace vms::server {

class Settings: public nx::utils::Settings
{
private:
    QString obtainDataDirectory()
    {
        if (m_dataDirectory.isNull())
        {
            #if defined(Q_OS_LINUX)
                m_dataDirectory = varDir();
            #else
                const QStringList& dataDirList =
                    QStandardPaths::standardLocations(QStandardPaths::DataLocation);
                m_dataDirectory = dataDirList.isEmpty() ? QString() : dataDirList[0];
            #endif
        }
        return m_dataDirectory;
    }

    QString m_dataDirectory;
    bool m_isBootedFromSdCard = false;

public:
    void setBootedFromSdCard(bool value) { m_isBootedFromSdCard = value; }

    // TODO: #lbusygin: Check int options for convertion to bool.
    Option<int> port{this, "port",
        7001,
        "Port, server listen on. All requests (ec2 API, mediaserver API, rtsp) are accepted on "
        "this port, so name \a rtspPort does not reflects its purpose"
    };
    Option<int> appserverPort{this, "appserverPort", 7001, ""};
    Option<QString> appserverHost{this, "appserverHost", "",
        "Application server IP address. Usually it's built in to a media server. So, value is "
        "empty or has value 'localhost'. But it's allowed to redirect media server to a remote "
        "database placed at another media server."
    };
    Option<QString> appserverPassword{this, "appserverPassword","",
        "Password to authenticate on EC database. If server use local database, this password is "
        "writen to a database as an admin password then it's removed from config for security "
        "reasons."
    };
    Option<QString> appserverLogin{this, "appserverLogin",helpers::kFactorySystemUser, ""};
    Option<qint64> minStorageSpace{this, "minStorageSpace",kDefaultMinStorageSpace, ""};
    Option<int> disableStorageDbOptimization{this, "disableStorageDbOptimization",0, ""};
    Option<int> maxRecordQueueSizeBytes{this, "maxRecordQueueSizeBytes", 1024 * 1024 * 20,
        "Maximum size of recording queue per camera stream. Queue overflow results in frame drops "
        "and \"Not enough HDD/SDD recording speed\" event."
    };
    Option<int> maxRecordQueueSizeElements{this, "maxRecordQueueSizeElements", 1000,
        "Maximum number of frames in recording queue per camera stream. Queue overflow results in "
        "frame drops and \"Not enough HDD/SDD recording speed\" event."
    };
    Option<unsigned int> hlsChunkCacheSizeSec{this, "hlsChunkCacheSizeSec", 60, ""};
    Option<int> hlsRemovedChunksToKeep{this, "hlsRemovedChunksToKeep",
        kDefaultHlsRemovedLiveChunksToKeep,
        "By hls specification, chunk, removed from playlist, SHOULD be available for period equal "
        "to chunk duration plus playlist duration.\n"
        "This results in 7-8 chunks (3-4 of them are already removed from playlist) are always in "
        "memory what can be too much for edge server.\n"
        "This setting allows to override that behavior and keep different removed chunks number "
        "in memory.\n"
        "-1 means spec-defined behavior is used (default), otherwise this is number of chunks "
        "removed from playlist which are still available for downloading\n"
        "\\warning Value of 0 is unreliable and can lead to improper playback (chunked removed "
        "after client has received playlist but before chunk has been downloaded), so 1 minimum "
        "is recommended"
    };
    Option<int> hlsPlaylistPreFillChunks{this, "hlsPlaylistPreFillChunks",
        kDefaultHlsPlaylistPreFillChunks,
        ""
    };
    Option<unsigned int> hlsMaxChunkBufferSize{this, "hlsMaxChunkBufferSize",
        kDefaultHlsMaxChunkBufferSize,
        "Chunk can be fully cached in memory only it size is not greater then this value. "
        "Otherwise, only last \a hlsMaxChunkBufferSize bytes can be stored in memory"
    };
    Option<int> ioBlockSize{this, "ioBlockSize",
        4 * 1024 * 1024,
        "Write block size. This block is always aligned to file system sector size"
    };
    Option<bool> disableDirectIO{this, "disableDirectIO", 0, ""};
    Option<int> ffmpegBufferSize{this, "ffmpegBufferSize",
        4 * 1024 * 1024,
        "Size of data to keep in memory after each write. This is required to minimize seeks on "
        "disk, since ffmpeg sometimes seeks to the left from current file position to fill in some"
        " media file structure size. Minimal value for adaptive buffer."
    };
    Option<int> maxFfmpegBufferSize{this, "maxFfmpegBufferSize", 4 * 1024 * 1024,
        "Size of data to keep in memory after each write. This is required to minimize seeks on "
        "disk, since ffmpeg sometimes seeks to the left from current file position to fill in some"
        " media file structure size. Minimal value for adaptive buffer."
    };
    Option<int> mediaFileDuration{this, "mediaFileDuration", 60, ""};
    Option<int> hlsInactivityPeriod{this, "hlsInactivityPeriod", 10,
        "If no one uses HLS for thid time period (in seconds), than live media cache is stopped "
        "and cleaned. It will be restarted with next HLS request"
    };
    Option<int> resourceInitThreadsCount{this, "resourceInitThreadsCount", 32, ""};
    Option<QString> allowedSslVersions{this, "allowedSslVersions", "",
        "Supported values: SSLv1, SSLv2, TLSv1, TLSv1.1, TLSv1.2"
    };
    Option<QString> allowedSslCiphers{this, "allowedSslCiphers", "",
        "Supported ciphers, see SSL docs."
    };
    Option<int> progressiveDownloadSessionLiveTimeSec {this,
        "progressiveDownloadSessionLiveTimeSec",
        0,
        "0 means no limit"
    };
    Option<bool> allowSslConnections{this, "allowSslConnections", true,
        "Either enable or not receive  ssl connection on the same TCP port. It's enabled by "
        "default."
    };
    Option<bool> createFullCrashDump{this, "createFullCrashDump", false,
        "Configures the size of crash dumps:\n"
        "Windows: If true, crash dump with contain full process memory included is created in case"
        " of medaserver crash. Otherwise, only stack trace of each thread is available in crash "
        "dump.\n"
        "Linux: If true, crash dumps are not affected (configured by kernel). Otherwise creates "
        "all threads backtrace text file (breaks real crash dump).\n"
        "Other platforms: Ignored (unsupported yet)."
    };
    Option<QString> publicIPServers{this, "publicIPServers", \
        "",
        "Server list via ';'. List of server for checking public IP address. Such server must "
        "return any HTTP page with IP address inside. It's extracted via regular expression."
    };
    Option<bool> ecDbReadOnly{this, "ecDbReadOnly",
        false,
        "If set to \a true, EC DB is opened in read-only mode. So, any "
        "data modification operation except license activation will fail",
        [this](bool value)
        {
            if (ecDbReadOnly.present())
                return value;

            #if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
                return m_isBootedFromSdCard;
            #endif

            return ecDbReadOnly.defaultValue();
        }
    };
    Option<QString> cdbEndpoint{this, "cdbEndpoint", "", ""};
    Option<QString> onvifTimeouts{this, "onvifTimeouts", "", ""};
    Option<int> enableMultipleInstances{this, "enableMultipleInstances", 0,
        "Enables multiple server instances to be started on the same machine. Each server should "
        "be started with its own config file and should have unique 'port', 'dataDir' and "
        "'serverGuid' values. If this option is turned on, then automatically found storages path "
        "will end with storages guid."
    };
    Option<QString> delayBeforeSettingMasterFlag{this, "delayBeforeSettingMasterFlag", "30s", ""};
    Option<bool> p2pMode{this, "p2pMode", true,
        "Switch data synchronization to the new optimized mode"
    };
    Option<int> allowRemovableStorages{this, "allowRemovableStorages", 1,
        "Allows automatic inclusion of discovered local removable storages into the resource pool."
    };
    Option<QString> checkForUpdateUrl{this, "checkForUpdateUrl",
        "http://updates.vmsproxy.com/updates.json",
        "Url used by mediaserver as a root update information source. Default = "
        "http://updates.vmsproxy.com.\n"
        "See: https://networkoptix.atlassian.net/wiki/spaces/PM/pages/197394433/Updates"
    };
    Option<bool> secureAppserverConnection{this, "secureAppserverConnection", false, ""};
    Option<bool> lowPriorityPassword{this, "lowPriorityPassword", false, ""};
    Option<QString> pendingSwitchToClusterMode{this, "pendingSwitchToClusterMode", "",
        "Temporary flag. Used on upgrading machine with mediaserver only. If set to true "
        "mediaserver connects to remote system, sets correct system name, removes this flag and "
        "restarts."
    };
    Option<bool> ffmpegRealTimeOptimization{this, "ffmpegRealTimeOptimization", true, ""};
    Option<int> redundancyTimeout{this, "redundancyTimeout", 3, ""};
    Option<QString> logDir{this, "logDir", "",
        "Directory to store logs in."
    };
    Option<QString> logLevel{this, "logLevel", "", ""};
    Option<QString> httpLogLevel{this, "http-log-level", "",
        "If not equal to none, http_log.log appears in logDir. All HTTP/RTSP requests/responses"
        " logged to that file."
    };
    Option<QString> systemLogLevel{this, "systemLogLevel", "", ""};
    Option<QString> tranLogLevel{this, "tranLogLevel", "",
        "If not equal to none, ec2_tran.log appears in logDir and ec2 transaction - related "
        "messages written to it."
    };
    Option<QString> permissionsLogLevel{this, "permissionsLogLevel", "", ""};
    Option<int> logArchiveSize{this, "logArchiveSize", 25,
        "Maximum number of log files of each type to store."
    };
    Option<unsigned int> maxLogFileSize{this, "maxLogFileSize", 10*1024*1024,
        "Maximum size (in bytes) of a single log file."
    };
    Option<int> tcpLogPort{this, "tcpLogPort", 0, ""};
    Option<int> noSetupWizard{this, "noSetupWizard", 0, ""};
    Option<int> publicIPEnabled{this, "publicIPEnabled", 1,
        "If true, allow server to discovery its public IP address. Default value is 'true'."
    };
    Option<QString> staticPublicIP{this, "staticPublicIP", "",
        "Set server public IP address manually. If it set server won't discovery it."
    };
    Option<QString> mediatorAddressUpdate{this, "mediatorAddressUpdate", "", ""};
    Option<bool> disableTranscoding{this, "disableTranscoding", false, ""};
    Option<bool> noResourceDiscovery{this, "noResourceDiscovery", false, ""};
    Option<bool> removeDbOnStartup{this, "removeDbOnStartup", false,
        "Cleanup ecs.sqlite when server is running if parameter is '1'. So, server will started "
        "with empty database. This parameter applies only once. Server will change it to '0' "
        "automatically."
    };
    Option<int> disableRename{this, "disableRename", 0, ""};
    Option<int> systemIdFromSystemName{this, "systemIdFromSystemName", 0, ""};
    Option<bool> noMonitorStatistics{this, "noMonitorStatistics",
        false,
        "disable CPU/network usage stats"
    };
    Option<QString> guidIsHWID{this, "guidIsHWID", "",
        "yes If current servers's hardware id is used as server id, no if not used. Initially this"
        " value is empty. On first start server checks if it can use hardwareid as its id and sets"
        " this flag to yes or no accordingly."
    };
    Option<QString> serverGuid{this, "serverGuid", "", ""};
    Option<QString> serverGuid2{this, "serverGuid2", "",
        "Server Guid used by v < 2.3 to connect to another server (workaround for DW's issue with "
        "cloning)."
    };
    Option<QString> obsoleteServerGuid{this, "obsoleteServerGuid", "",
        "Temporary variable. Set when changing serverGuid to hardware id."
    };
    Option<QString> if_{this, "if", "",
        "Allow only specific network adapter for a server"
    };
    Option<qint64> sysIdTime{this, "sysIdTime", 0, ""};
    Option<bool> authenticationEnabled{this, "authenticationEnabled", true, ""};
    Option<QByteArray> authKey{this, "authKey", "", ""};
    Option<qint64> forceStopRecordingTime{this, "forceStopRecordingTime",
        60 * 24 * 30,
        "Stop camera recording timeout if not enough license. Value in minutes. By default it's 30"
        " days.",
        [this](const qint64& value)
        {
            return qMin(value, forceStopRecordingTime.defaultValue());
        }
    };
    Option<int> maxConnections{this, "maxConnections", 2000, ""};
    Option<bool> noInitStoragesOnStartup{this, "noInitStoragesOnStartup", false, ""};
    Option<QString> ipVersion{this, "ipVersion", "", ""};
    Option<QString> rtspTransport{this, "rtspTransport", "tcp", ""};
    Option<bool> absoluteRtcpTimestamps{this, "absoluteRtcpTimestamps",
        true,
        "Enable absolute RTCP timestamps for archive data, RTCP NTP timestamps will corresond to "
        "media data absolute timestamps"
    };
    Option<bool> ignoreRootTool{this, "ignoreRootTool", false,
        "Ignore root tool executable presense (if set to true, media server will try to execute all "
        "commands that require root access directly)"};

#if defined(Q_OS_LINUX)
    Option<QString> varDir{this, "varDir",
        QString("/opt/%1/mediaserver/var").arg(QnAppInfo::linuxOrganizationName()),
        "Linux specific."
    };
#endif // defined(Q_OS_LINUX)

    Option<QString> sslCertificatePath{this, "sslCertificatePath",
        "",
        "Custom certificate path.",
        [this](const QString& value)
        {
            if (!sslCertificatePath.present())
                return dataDir() + lit( "/ssl/cert.pem");

            return value;
        }
    };
    Option<QString> dataDir{this, "dataDir",
        "",
        "/var (on linux). "
        "C:\\Users\\{username}\\AppData\\Local\\Network Optix\\Network Optix Media Server\\ on "
        "MSWin",
        [this](const QString& value)
        {
            if (!value.isEmpty())
                return value;

            return obtainDataDirectory();
        }
    };
    Option<QString> backupDir{this, "backupDir",
        "",
        "/opt/{CustomizationName}/mediaserver/var/backup (on linux). "
        "C:\\Users\\{username}\\AppData\\Local\\Network Optix\\Network Optix Media Server\\backup "
        "on MSWin",
        [this](const QString& value)
        {
            if (!value.isEmpty())
                return value;

            return closeDirPath(dataDir()) + "backup";
        }
    };
    Option<std::chrono::milliseconds> dbBackupPeriodMS{this, "dbBackupPeriodMS",
        kDbBackupPeriodHrs,
        "Backup EC database period. If server version is updated, this setting is neglected and "
        "backup is created after the updated server starts.",
        [](const std::chrono::milliseconds& value)
        {
            if (value.count() == 0)
                return std::chrono::milliseconds(kDbBackupPeriodHrs);

            return value;
        }
    };
    Option<QString> staticDataDir{this, "staticDataDir",
        "",
        "If set, license ecs_static.sqlite will be forwarded to specified directory.",
        [this](const QString& value)
        {
            if (!staticDataDir.present())
                return dataDir();

            return value;
        }
    };
    Option<QString> eventsDBFilePath{this, "eventsDBFilePath",
        "",
        "",
        [this](const QString& value)
        {
            if (!eventsDBFilePath.present())
                return dataDir();

            return value;
        }
    };
    Option<std::chrono::milliseconds> hlsTargetDurationMS{this, "hlsTargetDurationMS",
        kDefaultHlsTargetDurationMs,
        "Size of HLS chunk in millis. Specification recommends this value to be 10 seconds. Keep "
        "in mind that live playback delay is about 3 * hlsTargetDurationMS. Lower value can result"
        " in playback defects due to network congestion.",
        [](const std::chrono::milliseconds& value)
        {
            if (value.count() == 0)
                return kDefaultHlsTargetDurationMs;

            return value;
        }
    };
    Option<std::chrono::seconds> vacuumIntervalSec{this, "vacuumIntervalSec",
        kDefaultVacuumIntervalSecacuumIntervalSec,
        "Interval between storage media db vacuum routine is performed",
        [](const std::chrono::seconds& value)
        {
            if (value.count() == 0)
                return kDefaultVacuumIntervalSecacuumIntervalSec;

            return value;
        }
    };

#if defined(__arm__)
    static constexpr qint64 kDefaultMinStorageSpace = 100 * 1024 * 1024; //< 100MB
    static constexpr unsigned int kDefaultHlsMaxChunkBufferSize = 2 * 1024 * 1024;
#else // defined(__arm__)
    static constexpr qint64 kDefaultMinStorageSpace = 10 * 1024 * 1024 * 1024ll; //< 10GB
    static constexpr unsigned int kDefaultHlsMaxChunkBufferSize = 10 * 1024 * 1024;
#endif // defined(__arm__)
    static constexpr unsigned int kDefaultHlsPlaylistPreFillChunks = 2;
    static constexpr std::chrono::milliseconds kDefaultHlsTargetDurationMs{5 * 1000};
    static constexpr std::chrono::hours kDbBackupPeriodHrs{24};
    static constexpr int kDefaultHlsRemovedLiveChunksToKeep = -1;
    static constexpr std::chrono::seconds kDefaultVacuumIntervalSecacuumIntervalSec{3600 * 24};
};

} // namespace vms::server
} // namespace nx
