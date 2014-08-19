#ifndef _MEDIASERVER_SETTINGS_H_
#define _MEDIASERVER_SETTINGS_H_

#include <QtCore/QSettings>

//!Contains constants with names of configuration parameters
namespace nx_ms_conf
{
    //!Port, server listen on. All requests (ec2 API, mediaserver API, rtsp) are accepted on this port, so name \a rtspPort does not reflects its purpose
    static const QLatin1String RTSP_PORT( "port" );
    static const int DEFAULT_RTSP_PORT = 7001;

    static const QLatin1String MIN_STORAGE_SPACE( "minStorageSpace" );
#ifdef __arm__
    static const qint64 DEFAULT_MIN_STORAGE_SPACE =    100*1024*1024; // 100MB
#else
    static const qint64 DEFAULT_MIN_STORAGE_SPACE = 5*1024*1024*1024ll; // 5gb
#endif

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

    //!Port, onvif events are accepted on. Currently, only camera input events are handled. It is used with cameras that do not support onvif pullpoint subscription
    static const QLatin1String SOAP_PORT( "soapPort" );
    static const int DEFAULT_SOAP_PORT = 8083;

    //!Write block size. This block is always aligned to file system sector size
    static const QLatin1String IO_BLOCK_SIZE( "ioBlockSize" );
    static const int DEFAULT_IO_BLOCK_SIZE = 4*1024*1024;

    //!Size of data to keep in memory after each write
    /*!
        This required to minimize seeks on disk, since ffmpeg sometimes seeks to the left from current file position to fill in some media file structure size
    */
    static const QLatin1String FFMPEG_BUFFER_SIZE( "ffmpegBufferSize" );
    static const int DEFAULT_FFMPEG_BUFFER_SIZE = 4*1024*1024;

    //!If no one uses HLS for thid time period (in seconds), than live media cache is stopped and cleaned. It will be restarted with next HLS request
    static const QLatin1String HLS_INACTIVITY_PERIOD( "hlsInactivityPeriod" );
    static const int DEFAULT_HLS_INACTIVITY_PERIOD = 150;

    static const QLatin1String RESOURCE_INIT_THREADS_COUNT( "resourceInitThreadsCount" );
    static const int DEFAULT_RESOURCE_INIT_THREADS_COUNT = 64;

    static const QLatin1String SSL_CERTIFICATE_PATH( "sslCertificatePath" );

    static const QLatin1String PROGRESSIVE_DOWNLOADING_SESSION_LIVE_TIME( "progressiveDownloadSessionLiveTimeSec" );
    static const int DEFAULT_PROGRESSIVE_DOWNLOADING_SESSION_LIVE_TIME = 0;    //no limit
}

/*!
    QSettings instance is initialized in \a initializeROSettingsFromConfFile or \a initializeRunTimeSettingsFromConfFile call or on first demand
*/
class MSSettings
{
public:
    static QString defaultROSettingsFilePath();
    static void initializeROSettingsFromConfFile( const QString& fileName );
    static QSettings* roSettings();

    static QString defaultRunTimeSettingsFilePath();
    static void initializeRunTimeSettingsFromConfFile( const QString& fileName );
    static QSettings* runTimeSettings();
};

#endif
