#pragma once

#include <memory>
#include <QtGui/QRegion>
#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QSize>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/motion_window.h>
#include <nx/streaming/abstract_data_packet.h>
#include <motion/abstract_motion_archive.h>
#include <nx/utils/uuid.h>

#include "abstract_archive_integrity_watcher.h"
#include <utils/camera/camera_diagnostics.h>

enum class PlaybackMode
{
    Default,
    Live,
    Archive,
    ThumbNails,
    Export,
    Edge
};

class QnAbstractArchiveDelegate: public QObject
{
    Q_OBJECT
    Q_FLAGS(Flags Flag)

signals:
    // delete itself change quality
    void qualityChanged(MediaQuality quality);

public:
    struct ArchiveChunkInfo
    {
        qint64 startTimeUsec= -1;
        qint64 durationUsec = -1;
    };

    // TODO: #Elric #enum
    enum Flag
    {
        Flag_SlowSource = 1,
        Flag_CanProcessNegativeSpeed = 2,   // flag inform that delegate is going to process negative speed. If flag is not set, ArchiveReader is going to process negative speed
        Flag_CanProcessMediaStep = 4,       // flag inform that delegate is going to process media step itself.
        Flag_CanSendMetadata     = 8,       // supply metadata in the stream: motion, analytics e.t.c
        Flag_CanOfflineRange     = 16,      // delegate can return range immediately without opening archive
        Flag_CanSeekImmediatly   = 32,      // delegate can perform seek operation immediately, without 'open' function call
        Flag_CanOfflineLayout    = 64,      // delegate can return audio/video layout immediately without opening archive
        Flag_UnsyncTime          = 128,     // delegate may provide media data with not synced time (non equal to Server time)
        Flag_CanOfflineHasVideo  = 256      // delegate may provide info if media has video stream without opening
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QnAbstractArchiveDelegate() = default;
    virtual ~QnAbstractArchiveDelegate() override = default;

    virtual bool open(
        const QnResourcePtr& resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher = nullptr) = 0;
    virtual void close() = 0;
    virtual qint64 startTime() const = 0;
    virtual qint64 endTime() const = 0;
    virtual QnAbstractMediaDataPtr getNextData() = 0;
    // If findIFrame=true, jump to position before time to a nearest IFrame.
    /*!
       \ param time UTC, usec
        \param chunkInfo If not NULL, implementation fills this structure with info of chunk, containing found position
    */
    virtual qint64 seek (qint64 time, bool findIFrame) = 0;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() = 0;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() = 0;
    virtual bool hasVideo() const { return true; }

    virtual bool setAudioChannel(unsigned /*num*/) { return false; }

    // this call inform delegate that reverse mode on/off
    virtual void setSpeed(qint64 /*displayTime*/, double /*value*/) { }

    // for optimization. Inform delegate to pause after sending 1 frame
    virtual void setSingleshotMode(bool /*value*/) {}

    // MediaStreamQuality. By default, this function is not implemented. Return: true if need seek for change quality
    /*!
        \param fastSwitch Do not wait for next I-frame of new stream, but give data starting with previous I-frame
        \param resolution Is used only when quality == MEDIA_Quality_CustomResolution;
            width can be <= 0 which is treated as "auto".
    */
    virtual bool setQuality(MediaQuality /*quality*/, bool /*fastSwitch*/, const QSize& /*resolution*/ ) {  return false; }

    Flags getFlags() const { return m_flags; }
    virtual bool isRealTimeSource() const { return false; }
    virtual void beforeClose() {}

    /** This function calls from reader */
    virtual void beforeSeek(qint64 /*time*/) {}

    /** This function calls from reader */
    virtual void beforeChangeSpeed(double /*speed*/) {}

    /** This function used for thumbnails loader. Get data with specified media step from specified time interval*/
    virtual void setRange(qint64 /*startTime*/, qint64 /*endTime*/, qint64 /*frameStep*/) {}

    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int /*channel*/) { return nullptr; }
    virtual QnAbstractMotionArchiveConnectionPtr getAnalyticsConnection(int /*channel*/) { return nullptr; }

    virtual void setStreamDataFilter(nx::vms::api::StreamDataFilters /*filter*/) { }
    virtual nx::vms::api::StreamDataFilters streamDataFilter() const
    {
        return nx::vms::api::StreamDataFilter::mediaOnly;
    }

    virtual void setMotionRegion(const QnMotionRegion& /*region*/) {}

    /** This function used for multi-view delegate to help connect different streams together (VMAX) */
    virtual void setGroupId(const QByteArray& /*groupId*/) {}

    //!Returns information of chunk, used by previous \a QnAbstractArchiveDelegate::seek or \a QnAbstractArchiveDelegate::getNextData call
    virtual ArchiveChunkInfo getLastUsedChunkInfo() const { return ArchiveChunkInfo(); }

    virtual int getSequence() const { return 0;  }

    virtual void setPlaybackMode(PlaybackMode /*value*/) {}

    virtual void setEndOfPlaybackHandler(std::function<void()> handler)
    {
        m_endOfPlaybackHandler = handler;
    }

    virtual void setErrorHandler(std::function<void(const QString& errorString)> handler)
    {
        m_errorHandler = handler;
    }

    virtual CameraDiagnostics::Result lastError() const
    {
        return CameraDiagnostics::NoErrorResult();
    }

    /**
     * Stop consuming network resources e.t.c before long halt.
     * Reader should be able to restore its state on getNextPacket again.
     */
    virtual void pleaseStop() {}

protected:
    Flags m_flags = {};
    std::function<void()> m_endOfPlaybackHandler;
    std::function<void(const QString& errorString)> m_errorHandler;
};

typedef QSharedPointer<QnAbstractArchiveDelegate> QnAbstractArchiveDelegatePtr;
