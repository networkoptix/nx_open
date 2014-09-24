#ifndef __ABSTRACT_ARCHIVE_DELEGATE_H
#define __ABSTRACT_ARCHIVE_DELEGATE_H

#ifdef ENABLE_ARCHIVE

#include <QtGui/QRegion>
#include <QtCore/QObject>
#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>
#include "core/resource/resource_media_layout.h"
#include "core/datapacket/abstract_data_packet.h"
#include "motion/abstract_motion_archive.h"


class QnAbstractArchiveDelegate: public QObject
{
    Q_OBJECT;
    Q_FLAGS(Flags Flag);

signals:
    // delete itself change quality
    void qualityChanged(MediaQuality quality);

public:
    struct ArchiveChunkInfo
    {
        qint64 startTimeUsec;
        qint64 durationUsec;

        ArchiveChunkInfo() : startTimeUsec( -1 ), durationUsec( -1 ) {}
    };

    // TODO: #Elric #enum
    enum Flag 
    { 
        Flag_SlowSource = 1, 
        Flag_CanProcessNegativeSpeed = 2,   // flag inform that delegate is going to process negative speed. If flag is not set, ArchiveReader is going to process negative speed
        Flag_CanProcessMediaStep = 4,       // flag inform that delegate is going to process media step itself.
        Flag_CanSendMotion       = 8,       // motion supported
        Flag_CanOfflineRange     = 16,      // delegate can return range immediately without opening archive
        Flag_CanSeekImmediatly   = 32,      // delegate can perform seek operation immediately, without 'open' function call
        Flag_CanOfflineLayout    = 64,      // delegate can return audio/video layout immediately without opening archive
        Flag_UnsyncTime          = 128      // delegate may provide media data with not synced time (non equal to Server time)
    
    };
    Q_DECLARE_FLAGS(Flags, Flag);

    QnAbstractArchiveDelegate(): m_flags(0) {}
    virtual ~QnAbstractArchiveDelegate() {}

    virtual bool open(const QnResourcePtr &resource) = 0;
    virtual void close() = 0;
    virtual qint64 startTime() = 0;
    virtual qint64 endTime() = 0;
    virtual QnAbstractMediaDataPtr getNextData() = 0;
    // If findIFrame=true, jump to position before time to a nearest IFrame.
    /*!
       \ param time UTC, usec
        \param chunkInfo If not NULL, implementation fills this structure with info of chunk, containing found position
    */
    virtual qint64 seek (qint64 time, bool findIFrame) = 0;
    virtual QnResourceVideoLayoutPtr getVideoLayout() = 0;
    virtual QnResourceAudioLayoutPtr getAudioLayout() = 0;

    virtual AVCodecContext* setAudioChannel(int num) { Q_UNUSED(num); return 0; }
    
    // this call inform delegate that reverse mode on/off
    virtual void onReverseMode(qint64 displayTime, bool value) { Q_UNUSED(displayTime); Q_UNUSED(value); }

    // for optimization. Inform delegate to pause after sending 1 frame
    virtual void setSingleshotMode(bool value) { Q_UNUSED(value); }

    // MediaStreamQuality. By default, this function is not implemented. Return: true if need seek for change quality
    /*!
        \param fastSwitch Do not wait for next I-frame of new stream, but give data starting with previous I-frame
    */
    virtual bool setQuality(MediaQuality quality, bool fastSwitch) { Q_UNUSED(quality); Q_UNUSED(fastSwitch); return false; }

    Flags getFlags() const { return m_flags; }
    virtual bool isRealTimeSource() const { return false; }
    virtual void beforeClose() {}

    /** This function calls from reader */
    virtual void beforeSeek(qint64 time) { Q_UNUSED(time); }

    /** This function calls from reader */
    virtual void beforeChangeReverseMode(bool reverseMode) { Q_UNUSED(reverseMode); }

    /** This function used for thumbnails loader. Get data with specified media step from specified time interval*/
    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) { Q_UNUSED(startTime); Q_UNUSED(endTime); Q_UNUSED(frameStep); }

    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int channel) { Q_UNUSED(channel); return QnAbstractMotionArchiveConnectionPtr(); }

    virtual void setSendMotion(bool value) {Q_UNUSED(value); }

    /** This function used for multi-view delegate to help connect different streams together (VMAX) */
    virtual void setGroupId(const QByteArray& groupId) { Q_UNUSED(groupId); }

    //!Returns information of chunk, used by previous \a QnAbstractArchiveDelegate::seek or \a QnAbstractArchiveDelegate::getNextData call
    virtual ArchiveChunkInfo getLastUsedChunkInfo() const { return ArchiveChunkInfo(); };

protected:
    Flags m_flags;
};

typedef QSharedPointer<QnAbstractArchiveDelegate> QnAbstractArchiveDelegatePtr;

#endif // ENABLE_ARCHIVE

#endif
