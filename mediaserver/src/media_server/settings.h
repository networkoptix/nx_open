#ifndef _MEDIASERVER_SETTINGS_H_
#define _MEDIASERVER_SETTINGS_H_

#include <QtCore/QSettings>


//!Contains constants with names of configuration parameters
namespace nx_ms_conf
{
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
        This results in 7-8 chunks (3-4 of them are already removed from playlist) are always in memory what can be too hard for edge server.
        This setting allows to override that behavour and keep different removed chunks number in memory.
        \a -1 means spec-defined behavour is used (default), otherwise this is number of chunks removed from playlist which are still available for downloading
        \warning Value of 0 is unreliable and can lead to improper playback (chunked removed after client has received playlist but before chunk has been downloaded)
    */
    static const QLatin1String HLS_REMOVED_LIVE_CHUNKS_TO_KEEP( "hlsRemovedChunksToKeep" );
    static const unsigned int DEFAULT_HLS_REMOVED_LIVE_CHUNKS_TO_KEEP = -1;
}

/*!
    QSettings instance is initialized in \a initializeROSettingsFromConfFile or \a initializeRunTimeSettingsFromConfFile call or on first demand
*/
class MSSettings
{
public:
    static const int DEFAUT_RTSP_PORT = 7001;
    static const int DEFAULT_STREAMING_PORT = 7001;

    static QString defaultROSettingsFilePath();
    static void initializeROSettingsFromConfFile( const QString& fileName );
    static QSettings* roSettings();

    static QString defaultRunTimeSettingsFilePath();
    static void initializeRunTimeSettingsFromConfFile( const QString& fileName );
    static QSettings* runTimeSettings();
};

#endif
