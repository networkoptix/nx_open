#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QSettings>

#include <analytics/detected_objects_storage/analytics_events_storage_settings.h>

//!Contains constants with names of configuration parameters
namespace nx_ms_conf
{
    //!Port, server listen on. All requests (ec2 API, mediaserver API, rtsp) are accepted on this port, so name \a rtspPort does not reflects its purpose
    static const QLatin1String SERVER_PORT( "port" );
    static const int DEFAULT_SERVER_PORT = 7001;

    static const QLatin1String MIN_STORAGE_SPACE( "minStorageSpace" );
#ifdef __arm__
    static const qint64 DEFAULT_MIN_STORAGE_SPACE = 100*1024*1024; // 100MB
#else
    static const qint64 DEFAULT_MIN_STORAGE_SPACE = 10*1024*1024*1024ll; // 10gb
#endif

    static const QLatin1String DISABLE_STORAGE_DB_OPTIMIZATION("disableStorageDbOptimization");

	static const QLatin1String MAX_RECORD_QUEUE_SIZE_BYTES( "maxRecordQueueSizeBytes" );
    static const int DEFAULT_MAX_RECORD_QUEUE_SIZE_BYTES = 1024*1024*20;

    static const QLatin1String MAX_RECORD_QUEUE_SIZE_ELEMENTS( "maxRecordQueueSizeElements" );
    static const int DEFAULT_MAX_RECORD_QUEUE_SIZE_ELEMENTS = 1000;

    static const QLatin1String HLS_TARGET_DURATION_MS( "hlsTargetDurationMS" );
    static const unsigned int DEFAULT_TARGET_DURATION_MS = 5 * 1000;

    static const QLatin1String HLS_CHUNK_CACHE_SIZE_SEC( "hlsChunkCacheSizeSec" );
    static const unsigned int DEFAULT_MAX_CACHE_COST_SEC = 60;

    /*!
        By hls specification, chunk, removed from playlist, SHOULD be available for period equal to chunk duration plus playlist duration.
        This results in 7-8 chunks (3-4 of them are already removed from playlist) are always in memory what can be too much for edge server.
        This setting allows to override that behavior and keep different removed chunks number in memory.
        \a -1 means spec-defined behavior is used (default), otherwise this is number of chunks removed from playlist which are still available for downloading
        \warning Value of 0 is unreliable and can lead to improper playback (chunked removed after client has received playlist but before chunk has been downloaded),
            so 1 minimum is recommended
    */
    static const QLatin1String HLS_REMOVED_LIVE_CHUNKS_TO_KEEP( "hlsRemovedChunksToKeep" );
    static const unsigned int DEFAULT_HLS_REMOVED_LIVE_CHUNKS_TO_KEEP = -1;

    static const QLatin1String HLS_PLAYLIST_PRE_FILL_CHUNKS("hlsPlaylistPreFillChunks");
    static const unsigned int DEFAULT_HLS_PLAYLIST_PRE_FILL_CHUNKS = 2;

    /** Chunk can be fully cached in memory only it size is not greater then this value.
        Otherwise, only last \a DEFAULT_HLS_MAX_CHUNK_BUFFER_SIZE bytes can be stored in memory
    */
    static const QLatin1String HLS_MAX_CHUNK_BUFFER_SIZE("hlsMaxChunkBufferSize");
#ifdef __arm__
    static const unsigned int DEFAULT_HLS_MAX_CHUNK_BUFFER_SIZE = 2*1024*1024;
#else
    static const unsigned int DEFAULT_HLS_MAX_CHUNK_BUFFER_SIZE = 10*1024*1024;
#endif

    //!Write block size. This block is always aligned to file system sector size
    static const QLatin1String IO_BLOCK_SIZE( "ioBlockSize" );
    static const int DEFAULT_IO_BLOCK_SIZE = 4*1024*1024;

    static const QLatin1String DISABLE_DIRECT_IO ("disableDirectIO");

    //!Size of data to keep in memory after each write
    /*!
        This required to minimize seeks on disk, since ffmpeg sometimes seeks to the left from current file position to fill in some media file structure size
    */
    static const QLatin1String FFMPEG_BUFFER_SIZE( "ffmpegBufferSize" );
    static const int DEFAULT_FFMPEG_BUFFER_SIZE = 4*1024*1024;

    static const QLatin1String MAX_FFMPEG_BUFFER_SIZE( "maxFfmpegBufferSize" );
    static const int DEFAULT_MAX_FFMPEG_BUFFER_SIZE = 4*1024*1024;

    static const QLatin1String MEDIA_FILE_DURATION_SECONDS( "mediaFileDuration" );
    static const int DEFAULT_MEDIA_FILE_DURATION_SECONDS = 60;

    //!If no one uses HLS for thid time period (in seconds), than live media cache is stopped and cleaned. It will be restarted with next HLS request
    static const QLatin1String HLS_INACTIVITY_PERIOD( "hlsInactivityPeriod" );
    static const int DEFAULT_HLS_INACTIVITY_PERIOD = 10;

    static const QLatin1String RESOURCE_INIT_THREADS_COUNT( "resourceInitThreadsCount" );
    static const int DEFAULT_RESOURCE_INIT_THREADS_COUNT = 32;

    static const QLatin1String ALLOWED_SSL_VERSIONS( "allowedSslVersions" );
    static const QLatin1String ALLOWED_SSL_CIPHERS( "allowedSslCiphers" );
    static const QLatin1String SSL_CERTIFICATE_PATH( "sslCertificatePath" );

    static const QLatin1String PROGRESSIVE_DOWNLOADING_SESSION_LIVE_TIME( "progressiveDownloadSessionLiveTimeSec" );
    static const int DEFAULT_PROGRESSIVE_DOWNLOADING_SESSION_LIVE_TIME = 0;    //no limit

    static const QLatin1String ALLOW_SSL_CONNECTIONS( "allowSslConnections" );
    static const bool DEFAULT_ALLOW_SSL_CONNECTIONS = true;

    static const QLatin1String CREATE_FULL_CRASH_DUMP( "createFullCrashDump" );
    static const bool DEFAULT_CREATE_FULL_CRASH_DUMP = false;

    /** Semicolon-separated list of servers to get public ip. */
    static const QLatin1String PUBLIC_IP_SERVERS( "publicIPServers" );

    //!If set to \a true, EC DB is opened in read-only mode. So, any data modification operation except license activation will fail
    static const QLatin1String EC_DB_READ_ONLY( "ecDbReadOnly" );
    static const QLatin1String DEFAULT_EC_DB_READ_ONLY( "false" );

    static const QLatin1String CDB_ENDPOINT( "cdbEndpoint" );

    static const QLatin1String ONVIF_TIMEOUTS( "onvifTimeouts" );

    static const QLatin1String ENABLE_MULTIPLE_INSTANCES("enableMultipleInstances");

    static const QLatin1String DELAY_BEFORE_SETTING_MASTER_FLAG("delayBeforeSettingMasterFlag");
    static const QLatin1String DEFAULT_DELAY_BEFORE_SETTING_MASTER_FLAG("30s");
    static const QLatin1String P2P_MODE_FLAG("p2pMode");

    static const QLatin1String SYSTEM_USER("systemUser");

    static const QLatin1String CHECK_FOR_UPDATE_REFRESH_TIMEOUT("checkForUpdateRefreshTimeout");
}

/**
 * QSettings instance is initialized in \a initializeROSettingsFromConfFile or \a initializeRunTimeSettingsFromConfFile call or on first demand
 */
class MSSettings: public QObject
{
    Q_OBJECT;
public:
    MSSettings(
        const QString& roSettingsPath = QString(),
        const QString& rwSettingsPath = QString());

    QSettings* roSettings();
    const QSettings* roSettings() const;
    QSettings* runTimeSettings();
    const QSettings* runTimeSettings() const;

    QString getDataDirectory() const;

    std::chrono::milliseconds hlsTargetDuration() const;

    std::chrono::milliseconds delayBeforeSettingMasterFlag() const;

    nx::analytics::storage::Settings analyticEventsStorage() const;

    static QString defaultROSettingsFilePath();
    static QString defaultRunTimeSettingsFilePath();

private:
    void initializeROSettingsFromConfFile( const QString& fileName );
    void initializeROSettings();
    void initializeRunTimeSettingsFromConfFile( const QString& fileName );
    void initializeRunTimeSettings();

    void loadSettings();

    void loadGeneralSettings();
    QString loadDataDirectory();

    void loadAnalyticEventsStorageSettings();

private:
    std::unique_ptr<QSettings> m_rwSettings;
    std::unique_ptr<QSettings> m_roSettings;
    nx::analytics::storage::Settings m_analyticEventsStorage;
    QString m_dataDirectory;
};
