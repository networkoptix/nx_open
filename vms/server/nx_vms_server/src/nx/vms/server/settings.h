#pragma once

#include <QtCore/QStandardPaths>

#include <nx/utils/settings.h>
#include <network/multicodec_rtp_reader.h>
#include <decoders/video/abstract_video_decoder.h>
#include <network/system_helpers.h>
#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <server/server_globals.h>


// Type specific parsers
namespace nx::utils {

template<>
inline bool nx::utils::Settings::Option<MultiThreadDecodePolicy>::fromQVariant(
    const QVariant& value, MultiThreadDecodePolicy* result)
{
    if (!value.isValid())
        return false;

    QString valueString = value.toString();
    if (valueString == "auto")
        *result = MultiThreadDecodePolicy::autoDetect;
    else if (valueString == "disabled")
        *result = MultiThreadDecodePolicy::disabled;
    else if (valueString == "enabled")
        *result = MultiThreadDecodePolicy::enabled;
    else
        return false;
    return true;
}

template<>
inline QVariant Settings::Option<MultiThreadDecodePolicy>::toQVariant(
    const MultiThreadDecodePolicy& value)
{
    QVariant result;
    if (value == MultiThreadDecodePolicy::autoDetect)
        result.setValue(QString("auto"));
    if (value == MultiThreadDecodePolicy::disabled)
        result.setValue(QString("disabled"));
    if (value == MultiThreadDecodePolicy::enabled)
        result.setValue(QString("enabled"));
    return result;
}

}

namespace nx::vms::server {

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

    // TODO: #lbusygin: Check int options for conversion to bool.
    Option<int> port{this, "port",
        7001,
        "Port the Server listens for all requests (/ec2, /api, rtsp)."
    };
    Option<int> appserverPort{this, "appserverPort", 7001, ""};
    Option<QString> appserverHost{this, "appserverHost", "",
        "Application Server IP address. Usually it's built in into the Server. So, the value is "
        "empty or equals 'localhost'. But it's allowed to redirect the Server to a remote "
        "database located at another Server."
    };
    Option<QString> appserverPassword{this, "appserverPassword","",
        "Password to authenticate on EC database. If the Server uses the local database, this "
        "password is written to a database as an admin password, and then it's removed from the "
        "config for security reasons."
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
        "By HLS specification, a chunk removed from the playlist should be available for the "
        "period equal to the chunk duration plus the playlist duration.\n"
        "This results in 7-8 chunks (3-4 of them are already removed from the playlist) being "
        "always in memory, which can be too much for an Edge Server.\n"
        "This setting allows to override that behavior and keep different removed chunks number "
        "in memory.\n"
        "-1 means the spec-defined behavior is used (default), otherwise this is the number of "
        "chunks removed from the playlist which are still available for downloading.\n"
        "WARNING: The value of 0 is unreliable and can lead to improper playback (a chunk is "
        "removed after the Client has received playlist but before the chunk has been downloaded), so 1 minimum "
        "is recommended."
    };
    Option<int> hlsPlaylistPreFillChunks{this, "hlsPlaylistPreFillChunks",
        kDefaultHlsPlaylistPreFillChunks,
        ""
    };
    Option<unsigned int> hlsMaxChunkBufferSize{this, "hlsMaxChunkBufferSize",
        kDefaultHlsMaxChunkBufferSize,
        "Chunk can be fully cached in memory only it size is not greater then this value. "
        "Otherwise, only the last hlsMaxChunkBufferSize bytes can be stored in memory."
    };
    Option<int> ioBlockSize{this, "ioBlockSize",
        4 * 1024 * 1024,
        "Write block size. This block is always aligned to file system sector size"
    };
    Option<bool> disableDirectIO{this, "disableDirectIO", 0, ""};
    Option<int> ffmpegBufferSize{this, "ffmpegBufferSize",
        4 * 1024 * 1024,
        "Size of data to keep in memory after each write. This is required to minimize seeks on "
        "disk, since FFmpeg sometimes seeks to the left from current file position to fill in some "
        "media file structure size. Minimal value for adaptive buffer."
    };
    Option<int> maxFfmpegBufferSize{this, "maxFfmpegBufferSize", 4 * 1024 * 1024,
        "Size of data to keep in memory after each write. This is required to minimize seeks on "
        "disk, since FFmpeg sometimes seeks to the left from current file position to fill in some "
        "media file structure size. Minimal value for adaptive buffer."
    };
    Option<int> mediaFileDuration{this, "mediaFileDuration", 60, ""};
    Option<int> hlsInactivityPeriod{this, "hlsInactivityPeriod", 10,
        "If no one uses HLS for thid time period (in seconds), than live media cache is stopped "
        "and cleaned. It will be restarted with next HLS request."
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
        "Recommended TCP transport for the Server. Default value is true."
        "If this parameter is changed to false, other Servers will use HTTP "
        "instead of HTTPS when opening connections to this Server. "
        "It allows to save CPU for slow ARM devices."
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
        "If set to true, EC DB is opened in read-only mode. So, any "
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
        "Url used by the Server as a root update information source. Default: "
        "http://updates.vmsproxy.com."
    };
    Option<bool> secureAppserverConnection{this, "secureAppserverConnection", false, ""};
    Option<bool> lowPriorityPassword{this, "lowPriorityPassword", false, ""};
    Option<QString> pendingSwitchToClusterMode{this, "pendingSwitchToClusterMode", "",
        "Temporary flag. Used on upgrading machine with the Server only. If set to true, "
        "the Server connects to the remote system, sets correct system name, removes this flag, "
        "and restarts."
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
        "Disable CPU/network usage stats."
    };
    Option<QString> guidIsHWID{this, "guidIsHWID", "",
        "\"yes\" if the current Server's hardware id is used as the Server id, \"no\" otherwise. "
        "Initially this value is empty. On the first start, the Server checks if it can use the "
        "hardware id as its id, and sets this flag to \"yes\" or \"no\" accordingly."
    };
    Option<QString> serverGuid{this, "serverGuid", "", ""};
    Option<QString> serverGuid2{this, "serverGuid2", "",
        // Workaround for the old issue with cloning.
        "Deprecated. Server Guid used by v < 2.3 to connect to another Server."
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
        60 * 60 * 24 * 30,
        "Stop camera recording timeout if not enough licenses. Value in seconds. "
        "Default: 30 days.",
        [this](const qint64& value)
        {
            return qMin(value, forceStopRecordingTime.defaultValue());
        }
    };
    Option<int> maxConnections{this, "maxConnections", 2000, ""};
    Option<bool> noInitStoragesOnStartup{this, "noInitStoragesOnStartup", false, ""};
    Option<int> serverStartedEventTimeoutMs{this,
        QString::fromStdString(QnServer::serverStartedEventTimeoutMsName),
        3000, "Delay before generate serverStarted event on startup"};
    Option<bool> noPlugins{this, "noPlugins", false, "Turn off all plugins"};
    Option<QString> ipVersion{this, "ipVersion", "", ""};
    Option<QString> rtspTransport{this, "rtspTransport", "automatic", ""};
    Option<bool> absoluteRtcpTimestamps{this, "absoluteRtcpTimestamps",
        true,
        "Enable absolute RTCP timestamps for archive data, RTCP NTP timestamps will correspond to "
        "media data absolute timestamps."
    };
    Option<MultiThreadDecodePolicy> multiThreadDecodePolicy{this, "multiThreadDecodePolicy",
        MultiThreadDecodePolicy::autoDetect,
        "Multiple thread decoding policy {auto, disabled, enabled}, used for RTSP streaming with "
        "transcoding and motion estimation."
    };

    Option<bool> ignoreRootTool{this, "ignoreRootTool", false,
        "Ignore root tool executable presence (if set to true, the Server will try to execute all "
        "commands that require root access directly)."};

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
        "/var (on Linux), "
        "C:\\Users\\{username}\\AppData\\Local\\{vendor}}\\{product}\\ (on Windows).",
        [this](const QString& value)
        {
            if (!value.isEmpty())
                return value;

            return obtainDataDirectory();
        }
    };
    Option<QString> backupDir{this, "backupDir",
        "",
        "/opt/{vendor}/mediaserver/var/backup (on Linux), "
        "C:\\Users\\{username}\\AppData\\Local\\{vendor}}\\{server-product}\\backup (on Windows).",
        [this](const QString& value)
        {
            if (!value.isEmpty())
                return value;

            return closeDirPath(dataDir()) + "backup";
        }
    };
    Option<std::chrono::milliseconds> dbBackupPeriodMS{this, "dbBackupPeriodMS",
        kDbBackupPeriodHrs,
        "Backup EC database period. If Server version is updated, this setting is neglected and "
        "backup is created after the updated Server starts.",
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
        "Interval between storage media db vacuum routines.",
        [](const std::chrono::seconds& value)
        {
            if (value.count() == 0)
                return kDefaultVacuumIntervalSecacuumIntervalSec;

            return value;
        }
    };

    Option<int> retryCountToMakeCameraOffline{ this, "retryCountToMakeCameraOffline", 3,
        "How many discovery loops should pass before mark missed camera offline"
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
    static constexpr std::chrono::hours kDbBackupPeriodHrs{24 * 7};
    static constexpr int kDefaultHlsRemovedLiveChunksToKeep = -1;
    static constexpr std::chrono::seconds kDefaultVacuumIntervalSecacuumIntervalSec{3600 * 24};
};

} // namespace nx::vms::server
