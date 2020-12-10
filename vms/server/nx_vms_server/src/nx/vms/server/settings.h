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
namespace nx::utils::detail {

template<>
inline bool fromQVariant(const QVariant& value, MultiThreadDecodePolicy* result)
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
inline QVariant toQVariant(const MultiThreadDecodePolicy& value)
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

} // namespace nx::utils::detail

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
    Option<int> appserverPort{this, "appserverPort", 7001,
        "Proprietary (for internal use only)."
    };
    /**
     * Application Server IP address. It is built into the Server when this value is not specified,
     * empty, or equals "localhost". But it's allowed to redirect the Server to a remote database
     * located at another Server.
     */
    Option<QString> appserverHost{this, "appserverHost", "",
        "Proprietary (for internal use only)."
    };
    /**
     * Also this option is used when connecting to a database from a remote Server.
     */
    Option<QString> appserverPassword{this, "appserverPassword", "",
        "If specified, the administrator password will be set to this value, and this value will "
        "be removed (changed to empty) from the settings."
    };
    Option<QString> appserverLogin{this, "appserverLogin", helpers::kFactorySystemUser,
        "Proprietary (for internal use only)."
    };
    Option<qint64> minStorageSpace{this, "minStorageSpace",kDefaultMinStorageSpace,
        "The initial value of the minimum (reserved) storage space for a new storage; can be "
        "later changed in the settings for the particular storage."
    };
    Option<int> maxRecordQueueSizeBytes{this, "maxRecordQueueSizeBytes", 1024 * 1024 * 20,
        "Maximum size of recording queue per camera stream. Queue overflow results in frame drops "
        "and \"Not enough HDD/SDD recording speed\" event."
    };
    Option<int> maxRecordQueueSizeElements{this, "maxRecordQueueSizeElements", 1000,
        "Maximum number of frames in recording queue per camera stream. Queue overflow results in "
        "frame drops and \"Not enough HDD/SDD recording speed\" event."
    };
    Option<unsigned int> hlsChunkCacheSizeSec{this, "hlsChunkCacheSizeSec", 60,
        "Proprietary (for internal use only)."
    };
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
        "removed after the Client has received playlist but before the chunk has been "
        "downloaded), so a minimum value of 1 is recommended."
    };
    Option<int> hlsPlaylistPreFillChunks{this, "hlsPlaylistPreFillChunks",
        kDefaultHlsPlaylistPreFillChunks,
        "Proprietary (for internal use only)."
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
    Option<bool> disableDirectIO{this, "disableDirectIO", false,
        "In Windows, in case of low disk performance (high disk usage), allows to disable the "
        "mode of working with the files."};
    Option<int> ffmpegBufferSize{this, "ffmpegBufferSize", 4 * 1024 * 1024,
        "The minimum size of the adaptive FFmpeg buffer. This buffer keeps the last written "
        "stream data in memory to minimize disk seeks, because FFmpeg sometimes seeks to the left "
        "to fill in a size field of a media structure."
    };
    Option<int> maxFfmpegBufferSize{this, "maxFfmpegBufferSize", 4 * 1024 * 1024,
        "The maximum size of the adaptive FFmpeg buffer. This buffer keeps the last written "
        "stream data in memory to minimize disk seeks, because FFmpeg sometimes seeks to the left "
        "to fill in a size field of a media structure."
    };
    Option<int> mediaFileDuration{this, "mediaFileDuration", 60,
        "average file duration in seconds for recording"
    };
    Option<int> hlsInactivityPeriod{this, "hlsInactivityPeriod", 10,
        "If no-one uses HLS for this time period (in seconds), then the live media cache is "
        "stopped and cleaned. It will be restarted with the next HLS request."
    };
    Option<int> resourceInitThreadsCount{this, "resourceInitThreadsCount", 64,
        "The maximum number of cameras that the Server is allowed to initialize in parallel."
    };
    Option<QString> allowedSslVersions{this, "allowedSslVersions", "",
        "Supported values: SSLv1, SSLv2, TLSv1, TLSv1.1, TLSv1.2. If not specified, allowed SSL "
        "versions will not be set."
    };
    Option<QString> allowedSslCiphers{this, "allowedSslCiphers", "",
        "Supported ciphers, see SSL docs. If not specified, ciphers will not be set."
    };
    Option<int> progressiveDownloadSessionLiveTimeSec {this,
        "progressiveDownloadSessionLiveTimeSec",
        0,
        "0 means no limit"
    };
    Option<int> mediaStatisticsWindowSize{this, "mediaStatisticsWindowSize", 10,
        "Time period in seconds for media stream statistics."
    };
    Option<int> mediaStatisticsMaxDurationInFrames{this, "mediaStatisticsMaxDurationInFrames", 0,
        "Maximum queue size in media frames for media stream statistics."
        "Value 0 means unlimited."
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
    Option<QString> publicIPServers{this, "publicIPServers",
        "",
        "Server list via ';'. List of Servers for checking public IP address. Such Servers must "
        "return any HTTP page with IP address inside. It's extracted via a regular expression."
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
    Option<QString> cdbEndpoint{this, "cdbEndpoint", "",
        "Proprietary (for internal use only)."
    };
    Option<QString> onvifTimeouts{this, "onvifTimeouts", "",
        "Set the timeout values for different ONVIF operations when the Server communicates with "
        "an ONVIF camera, in seconds. Can be either one number which sets all timeouts to this "
        "value, or a semicolon-separated list of 4 values which set the timeouts for sending, "
        "receiving, connecting, and accepting operation respectively. If not specified, all "
        "timeout values default to 10 seconds."
    };
    Option<int> enableMultipleInstances{this, "enableMultipleInstances", 0,
        "Enables multiple server instances to be started on the same machine. Each server should "
        "be started with its own config file and should have unique 'port', 'dataDir' and "
        "'serverGuid' values. If this option is turned on, then automatically found storages path "
        "will end with storages guid."
    };
    /**
     * When there are many Servers, only one must connect to the Cloud. The algorithm selecting
     * the Servers uses this setting to vote.
     */
    Option<QString> delayBeforeSettingMasterFlag{this, "delayBeforeSettingMasterFlag", "30s",
        "Proprietary (for internal use only)."
    };
    Option<bool> p2pMode{this, "p2pMode", true,
        "Switch data synchronization to the new optimized mode"
    };
    Option<int> allowRemovableStorages{this, "allowRemovableStorages", 0,
        "Allows automatic inclusion of discovered local removable storages into the resource pool."
    };
    Option<QString> checkForUpdateUrl{this, "checkForUpdateUrl",
        "https://updates.vmsproxy.com/updates.json",
        "Url used by the Server as a root update information source. Default: "
        "https://updates.vmsproxy.com."
    };
    Option<bool> secureAppserverConnection{this, "secureAppserverConnection", false,
        "Proprietary (for internal use only)."
    };
    /** Used as a temporary storage. */
    Option<bool> lowPriorityPassword{this, "lowPriorityPassword", false,
        "Proprietary (for internal use only)."
    };
    Option<QString> pendingSwitchToClusterMode{this, "pendingSwitchToClusterMode", "",
        "Temporary flag. Used on upgrading machine with the Server only. If set to true, "
        "the Server connects to the remote system, sets correct system name, removes this flag, "
        "and restarts."
    };
    Option<bool> ffmpegRealTimeOptimization{this, "ffmpegRealTimeOptimization", true,
        "If set, when the Server uses video transcoding, for certain formats (like WebM), in the "
        "case of too high CPU usage, the Server will drop frames in order to catch up."
    };
    Option<int> redundancyTimeout{this, "redundancyTimeout", 3,
        "Server inactivity time, in seconds, after which the Server decides to take a camera from "
        "another Server which has been offline."
    };
    Option<QString> logDir{this, "logDir", "",
        "Directory to store the logs in. If not specified, the following directories are used: "
        "/opt/{vendor}/mediaserver/var/log (on Linux), "
        "C:\\Users\\{username}\\AppData\\Local\\{vendor}\\{product}\\log (on Windows). "
        "NOTE: Another setting \"logFile\", which may be observed "
        "among the settings, cannot be used to set the Server main log file location - it is "
        "intended for internal use, contains a generated value with the actual main log file "
        "location, and thus is not documented here."
    };
    /**
     * NOTE: Log level "always" is not documented here because it is deprecated. The same is for
     * the legacy log level "debug2" which has been renamed to "verbose" but is still supported.
     */
    Option<QString> logLevel{this, "logLevel", "",
        "Log level for the main Server log (log_file.log). Can be either set to one of the values "
        "shown below, or to a comma-separated list of log levels for the particular log tags "
        "which start with the specified prefixes: \"<level>[<tag-prefix>],...\". The following "
        "log levels are supported and can be specified in a case-insensitive way either by their "
        "full name or by its first letter: \"error\", \"warning\", \"info\", \"debug\", "
        "\"verbose\", \"none\" (in this case the log file will not be created). If this option is "
        "not specified, the main log level will be set to \"info\"."
    };
    Option<QString> httpLogLevel{this, "http-log-level", "",
        "If not equal to \"none\", http_log.log appears in logDir. All HTTP/RTSP "
        "requests/responses logged to that file."
    };
    Option<QString> systemLogLevel{this, "systemLogLevel", "",
        "Log level for the System log. See \"logLevel\" option description for possible values. "
        "If not specified, \"none\" is assumed."
    };
    Option<QString> tranLogLevel{this, "tranLogLevel", "",
        "If not equal to \"none\", ec2_tran.log appears in logDir and ec2-transaction-related "
        "messages are written to it. See \"logLevel\" option description for possible values. "
        "If not specified, \"none\" is assumed."
    };
    Option<QString> permissionsLogLevel{this, "permissionsLogLevel", "",
        "Log level for the Permissions log. See \"logLevel\" option description for possible "
        "values. If not specified, \"none\" is assumed."
    };
    Option<int> logArchiveSize{this, "logArchiveSize", 25,
        "Maximum number of log files of each type to store."
    };
    Option<unsigned int> maxLogFileSize{this, "maxLogFileSize", 10*1024*1024,
        "Maximum size (in bytes) of a single log file."
    };
    Option<int> tcpLogPort{this, "tcpLogPort", 0, ""};
    Option<int> noSetupWizard{this, "noSetupWizard", 0,
        "Proprietary (for internal use only)."
    };
    Option<int> publicIPEnabled{this, "publicIPEnabled", 1,
        "If true, allow server to discovery its public IP address. Default value is 'true'."
    };
    Option<int> onlineResourceDataEnabled{this, "onlineResourceDataEnabled", 1,
        "If true, allow to update camera configuration file (resource_data.json) online."
    };
    Option<QString> staticPublicIP{this, "staticPublicIP", "",
        "Set server public IP address manually. If it is set, the Server will not discover it."
    };
    Option<QString> mediatorAddressUpdate{this, "mediatorAddressUpdate", "",
        "Proprietary (for internal use only)."
    };
    Option<bool> disableTranscoding{this, "disableTranscoding", false,
        "Disable video transcoding in the case the Client requests codec/resolution different "
        "from the one that has come from a camera."
    };
    Option<bool> allowThirdPartyProxy{this, "allowThirdPartyProxy", false,
        "Allow proxy requests for any sites if value is true. "
        "Allow proxy requests for known servers and cameras only if it is false."
    };
    Option<bool> noResourceDiscovery{this, "noResourceDiscovery", false,
        "Proprietary (for internal use only)."
    };
    Option<bool> removeDbOnStartup{this, "removeDbOnStartup", false,
        "If set, clean up ecs.sqlite (the main Server database) when the Server is started. So, "
        "the Server will start with the empty database. This parameter applies only once. The "
        "Server will change it to '0' (false) automatically."
    };
    /** Rename video archive adding the duration after the recording. */
    Option<int> disableRename{this, "disableRename", 0,
        "Proprietary (for internal use only)."
    };
    /** Used for compatibility with old Servers. */
    Option<int> systemIdFromSystemName{this, "systemIdFromSystemName", 0,
        "Proprietary (for internal use only)."
    };
    Option<bool> noMonitorStatistics{this, "noMonitorStatistics",
        false,
        "Disable CPU/network usage stats."
    };
    Option<QString> guidIsHWID{this, "guidIsHWID", "",
        "\"yes\" if the current Server's hardware id is used as the Server id, \"no\" otherwise. "
        "Initially this value is empty. On the first start, the Server checks if it can use the "
        "hardware id as its id, and sets this flag to \"yes\" or \"no\" accordingly."
    };
    Option<QString> serverGuid{this, "serverGuid", "",
        "Proprietary (for internal use only)."
    };
    /**
     * Workaround for the old issue with cloning. Deprecated. Server Guid used by v < 2.3 to
     * connect to another Server.
     */
    Option<QString> serverGuid2{this, "serverGuid2", "",
        "Proprietary (for internal use only)."
    };
    /** Temporary variable. Set when changing serverGuid to hardware id. */
    Option<QString> obsoleteServerGuid{this, "obsoleteServerGuid", "",
        "Proprietary (for internal use only)."
    };
    Option<QString> if_{this, "if", "",
        "Allow only specific network adapter(s) for the Server. Contains a semicolon-separated "
        "list of hosts/IPs. If not specified, no limitation is in effect."
    };
    Option<qint64> sysIdTime{this, "sysIdTime", 0,
        "Proprietary (for internal use only)."
    };
    Option<bool> authenticationEnabled{this, "authenticationEnabled", true,
        "Proprietary (for internal use only)."
    };
    Option<QByteArray> authKey{this, "authKey", "",
        "Proprietary (for internal use only)."
    };
    Option<qint64> forceStopRecordingTime{this, "forceStopRecordingTime",
        60 * 60 * 24 * 30,
        "Stop camera recording timeout if not enough licenses. Value in seconds. "
        "Default: 30 days.",
        [this](const qint64& value)
        {
            return qMin(value, forceStopRecordingTime.defaultValue());
        }
    };
    Option<qint64> forceStopVideoWallTime{this, "forceStopVideoWallTime",
        60 * 60 * 24 * 7,
        "Stop video wall timeout if not enough licenses. Value in seconds. "
        "Default: 7 days.",
        [this](const qint64& value)
        {
            return qMin(value, forceStopVideoWallTime.defaultValue());
        }
    };
    Option<int> maxConnections{this, "maxConnections", 2000,
        "The maximum number of simultaneous incoming TCP connections for the Server."
    };
    Option<bool> useTwoSockets{this, "useTwoSockets", true,
        "Listen server port using ipv4 and ipv6 sockets. Use ipv6 only if the value is false."
    };
    Option<bool> noInitStoragesOnStartup{this, "noInitStoragesOnStartup", false,
        "Proprietary (for internal use only)."
    };
    Option<int> serverStartedEventTimeoutMs{this,
        QString::fromStdString(QnServer::serverStartedEventTimeoutMsName),
        3000, "Delay before generate serverStarted event on startup"
    };
    Option<bool> noPlugins{this, "noPlugins", false, "Turn off all plugins"};
    Option<QString> ipVersion{this, "ipVersion", "",
        "Proprietary (for internal use only)."
    };
    Option<QString> rtspTransport{this, "rtspTransport", "Auto",
        "The initial selection of the protocol used behind the RTSP communication for new "
        "cameras; can be later changed in camera settings. Possible values are: \"TCP\", "
        "\"UDP\", \"Multicast\", and \"Auto\"."
    };
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
    Option<bool> allowGlobalLumaFiltering{this, "allowGlobalLumaFiltering",
        true,
        "Allow global luminance change detection across whole frame for motion estimation"
    };
    Option<bool> ignoreRootTool{this, "ignoreRootTool", false,
        "Ignore root tool executable presence (if set to true, the Server will try to execute all "
        "commands that require root access directly)."
    };

#if defined(Q_OS_LINUX)
    Option<QString> varDir{this, "varDir",
        QString("/opt/%1/mediaserver/var").arg(QnAppInfo::linuxOrganizationName()),
        "Linux specific."
    };
#endif // defined(Q_OS_LINUX)

    Option<QString> sslCertificatePath{this, "sslCertificatePath",
        "",
        "Path to the SSL certificate file. If not specified, the SSL certificate will not be "
        "loaded.",
        [this](const QString& value)
        {
            if (!sslCertificatePath.present())
                return dataDir() + lit( "/ssl/cert.pem");

            return value;
        }
    };
    Option<QString> dataDir{this, "dataDir",
        "",
        "Directory where the Server stores its runtime data. If not specified, the following "
        "directories are used: /opt/{vendor}/mediaserver/var (on Linux), "
        "C:\\Users\\{username}\\AppData\\Local\\{vendor}\\{product} (on Windows).",
        [this](const QString& value)
        {
            if (!value.isEmpty())
                return value;

            return obtainDataDirectory();
        }
    };
    Option<QString> backupDir{this, "backupDir",
        "",
        "Directory for the Server database backup. If not specified, the following directories "
        "are used: /opt/{vendor}/mediaserver/var/backup (on Linux), "
        "C:\\Users\\{username}\\AppData\\Local\\{vendor}\\{server-product}\\backup (on Windows).",
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
        "Directory where mserver.sqlite database file resides. If not specified, it will be "
        "located in the same directory as ecs.sqlite (the main Server database).",
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
    Option<int> retryCountToMakeCameraOffline{this, "retryCountToMakeCameraOffline", 3,
        "How many discovery loops should pass before mark missed camera offline"
    };
    Option<qint64> minSystemStorageFreeSpace{this,
        "minSystemStorageFreeSpace", kMinSystemStorageFreeSpace,
        "The minimum allowed system storage free space, in bytes. If it less, the Server will "
        "generate a warning Event."
    };
    Option<bool> noOutgoingConnectionsMetric{this, "noOutgoingConnectionsMetric", false,
        "Disable metric 'outgoingConnections'. Used for test purpose only."
    };
    Option<bool> allowSystemStorageRecording{this, "allowSystemStorageRecording", true,
        "Toggles recording on/off for system storage."
    };
    Option<std::chrono::seconds> ioOperationTimeTresholdSec{this, "ioOperationTimeTresholdSec",
        kDefaultIoOperationTimeTresholdSec,
        "If IO operation (read/write/remove/list) takes time longer than this value it will be reported in server metrics.",
        [](const std::chrono::seconds& value)
        {
            if (value.count() == 0)
                return kDefaultIoOperationTimeTresholdSec;

            return value;
        }
    };

    Option<QString> currentOsVariantOverride{this, "currentOsVariantOverride", "",
        "Overrides the detected OS variant value (e.g. \"ubuntu\")."
    };
    Option<QString> currentOsVariantVersionOverride{this, "currentOsVariantVersionOverride", "",
        "Overrides the detected OS variant version value (e.g. \"16.04\")."
    };

    Option<QString> additionalUpdateVerificationKeysDir{this,
        "additionalUpdateVerificationKeysDir", "",
        "Path to a directory where additional keys for update files verification are located."
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
    static constexpr std::chrono::seconds kDefaultIoOperationTimeTresholdSec{5};

    #if defined(__arm__)
        static constexpr qint64 kMinSystemStorageFreeSpace = 500 * 1000 * 1000LL;
    #else
        static constexpr qint64 kMinSystemStorageFreeSpace = 5000 * 1000 * 1000LL;
    #endif
};

} // namespace nx::vms::server
