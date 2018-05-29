#pragma once

#include <nx/utils/settings.h>
#include <network/multicodec_rtp_reader.h>
#include <nx/update/info/async_update_checker.h>
#include <network/system_helpers.h>
#include <utils/common/app_info.h>

namespace nx {
namespace mediaserver {

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

public:
    // TODO: #lbusygin: Check int options for convertion to bool.
    Option<int> port{this, "port",
        7001,
        "Port, server listen on. All requests (ec2 API, mediaserver API, rtsp) are accepted on "
        "this port, so name \a rtspPort does not reflects its purpose"
    };
    Option<int> appserverPort{this, "appserverPort", 7001, ""};
    Option<QString> appserverHost{this, "appserverHost", "", ""};
    Option<QString> appserverPassword{this, "appserverPassword","", ""};
    Option<QString> appserverLogin{this, "appserverLogin",helpers::kFactorySystemUser, ""};
    Option<qint64> minStorageSpace{this, "minStorageSpace",kDefaultMinStorageSpace, ""};
    Option<int> disableStorageDbOptimization{this, "disableStorageDbOptimization",0, ""};
    Option<int> maxRecordQueueSizeBytes{this, "maxRecordQueueSizeBytes", 1024 * 1024 * 20, ""};
    Option<int> maxRecordQueueSizeElements{this, "maxRecordQueueSizeElements", 1000, ""};
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
        "is ecommended"
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
        "This required to minimize seeks on disk, since ffmpeg sometimes seeks to the left from "
        "current file position to fill in some media file structure size"
    };
    Option<int> maxFfmpegBufferSize{this, "maxFfmpegBufferSize", 4 * 1024 * 1024, ""};
    Option<int> mediaFileDuration{this, "mediaFileDuration", 60, ""};
    Option<int> hlsInactivityPeriod{this, "hlsInactivityPeriod", 10,
        "If no one uses HLS for thid time period (in seconds), than live media cache is stopped "
        "and cleaned. It will be restarted with next HLS request"
    };
    Option<int> resourceInitThreadsCount{this, "resourceInitThreadsCount", 32, ""};
    Option<QString> allowedSslVersions{this, "allowedSslVersions", "", ""};
    Option<QString> allowedSslCiphers{this, "allowedSslCiphers", "", ""};
    Option<int> progressiveDownloadSessionLiveTimeSec {this,
        "progressiveDownloadSessionLiveTimeSec",
        0,
        ""
    };
    Option<bool> allowSslConnections{this, "allowSslConnections", true, ""};
    Option<bool> createFullCrashDump{this, "createFullCrashDump", false, ""};
    Option<QString> publicIPServers{this, "publicIPServers", \
        "",
        "Semicolon-separated list of servers to get public ip."
    };
    Option<bool> ecDbReadOnly{this, "ecDbReadOnly",
        false,
        "If set to \a true, EC DB is opened in read-only mode. So, any "
        "data modification operation except license activation will fail"
    };
    Option<QString> cdbEndpoint{this, "cdbEndpoint", "", ""};
    Option<QString> onvifTimeouts{this, "onvifTimeouts", "", ""};
    Option<int> enableMultipleInstances{this, "enableMultipleInstances", 0, ""};
    Option<QString> delayBeforeSettingMasterFlag{this, "delayBeforeSettingMasterFlag", "30s", ""};
    Option<bool> p2pMode{this, "p2pMode", false, ""};
    Option<int> allowRemovableStorages{this, "allowRemovableStorages", 1, ""};
    Option<QString> checkForUpdateUrl{this, "checkForUpdateUrl",
        nx::update::info::kDefaultUrl,
        ""};
    Option<bool> secureAppserverConnection{this, "secureAppserverConnection", false, ""};
    Option<bool> lowPriorityPassword{this, "lowPriorityPassword", false, ""};
    Option<QString> pendingSwitchToClusterMode{this, "pendingSwitchToClusterMode", "", ""};
    Option<bool> ffmpegRealTimeOptimization{this, "ffmpegRealTimeOptimization", true, ""};
    Option<int> redundancyTimeout{this, "redundancyTimeout", 3, ""};
    Option<QString> logDir{this, "logDir", "", ""};
    Option<QString> logLevel{this, "logLevel", "", ""};
    Option<QString> httpLogLevel{this, "http-log-level", "", ""};
    Option<QString> systemLogLevel{this, "systemLogLevel", "", ""};
    Option<QString> tranLogLevel{this, "tranLogLevel", "", ""};
    Option<QString> permissionsLogLevel{this, "permissionsLogLevel", "", ""};
    Option<int> logArchiveSize{this, "logArchiveSize", 25, ""};
    Option<unsigned int> maxLogFileSize{this, "maxLogFileSize", 10*1024*1024, ""};
    Option<int> tcpLogPort{this, "tcpLogPort", 0, ""};
    Option<int> noSetupWizard{this, "noSetupWizard", 0, ""};
    Option<int> publicIPEnabled{this, "publicIPEnabled", 1, ""};
    Option<QString> staticPublicIP{this, "staticPublicIP", "", ""};
    Option<QString> mediatorAddressUpdate{this, "mediatorAddressUpdate", "", ""};
    Option<bool> disableTranscoding{this, "disableTranscoding", false, ""};
    Option<bool> noResourceDiscovery{this, "noResourceDiscovery", false, ""};
    Option<bool> removeDbOnStartup{this, "removeDbOnStartup", false, ""};
    Option<int> disableRename{this, "disableRename", 0, ""};
    Option<int> systemIdFromSystemName{this, "systemIdFromSystemName", 0, ""};
    Option<bool> noMonitorStatistics{this, "noMonitorStatistics",
        false,
        "disable CPU/network usage stats"
    };
    Option<QString> guidIsHWID{this, "guidIsHWID", "", ""};
    Option<QString> serverGuid{this, "serverGuid", "", ""};
    Option<QString> serverGuid2{this, "serverGuid2", "", ""};
    Option<QString> obsoleteServerGuid{this, "obsoleteServerGuid", "", ""};

    Option<QString> if_{this, "if", "", ""};
    Option<qint64> sysIdTime{this, "sysIdTime", 0, ""};
    Option<bool> authenticationEnabled{this, "authenticationEnabled", true, ""};
    Option<QByteArray> authKey{this, "authKey", "", ""};
    Option<qint64> forceStopRecordingTime{this, "forceStopRecordingTime",
        60 * 24 * 30,
        "",
        [this](const qint64& value)
        {
            return qMin(value, forceStopRecordingTime.defaultValue());
        }
    };
    Option<int> maxConnections{this, "maxConnections", 2000, ""};
    Option<bool> noInitStoragesOnStartup{this, "noInitStoragesOnStartup", false, ""};
    Option<QString> ipVersion{this, "ipVersion", "", ""};
    Option<QString> rtspTransport{this, "rtspTransport", RtpTransport::_auto, ""};

#if defined(Q_OS_LINUX)
    Option<QString> varDir{this, "varDir",
        QString("/opt/%1/mediaserver/var").arg(QnAppInfo::linuxOrganizationName()),
        ""
    };
#endif // defined(Q_OS_LINUX)

    Option<QString> sslCertificatePath{this, "sslCertificatePath",
        "",
        "",
        [this](const QString& value)
        {
            if (!sslCertificatePath.present())
                return dataDir() + lit( "/ssl/cert.pem");

            return value;
        }
    };
    Option<QString> dataDir{this, "dataDir",
        "",
        "",
        [this](const QString& value)
        {
            if (!value.isEmpty())
                return value;

            return obtainDataDirectory();
        }
    };
    Option<QString> staticDataDir{this, "staticDataDir",
        "",
        "",
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
        "",
        [](const std::chrono::milliseconds& value)
        {
            if (value.count() == 0)
                return kDefaultHlsTargetDurationMs;

            return value;
        }
    };
    Option<qint64> checkForUpdateTimeout{this, "checkForUpdateTimeout",
        0, //< TODO: #lbusygin: Investiagate documentation inconsistency.
        "",
        [](const qint64& value)
        {
            if (value == 0)
                return 10 * 60 * 1000ll;

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
    static constexpr int kDefaultHlsRemovedLiveChunksToKeep = -1;
};

} // namespace mediaserver
} // namespace nx
