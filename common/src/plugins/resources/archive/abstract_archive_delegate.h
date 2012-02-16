#ifndef __ABSTRACT_ARCHIVE_DELEGATE_H
#define __ABSTRACT_ARCHIVE_DELEGATE_H

#include <QRegion>
#include <QObject>
#include <QVector>
#include "core/resource/resource.h"
#include "core/datapacket/mediadatapacket.h"

class QnVideoResourceLayout;
class QnResourceAudioLayout;

class QnAbstractFilterPlaybackDelegate
{
    public:
        virtual void setMotionRegion(const QRegion& region) = 0;
        virtual void setSendMotion(bool value) = 0;
};

class QnAbstractArchiveDelegate: public QObject
{
    Q_OBJECT;
    Q_FLAGS(Flags Flag);

signals:
    // delete itself change quality
    void qualityChanged(MediaQuality quality);
public:
    enum Flag 
    { 
        Flag_SlowSource = 1, 
        Flag_CanProcessNegativeSpeed = 2 // flag inform that delegate is going to process negative speed. If flag is not setted, ArchiveReader is going to process negative speed
    };
    Q_DECLARE_FLAGS(Flags, Flag);

    QnAbstractArchiveDelegate(): m_flags(0) {}
    virtual ~QnAbstractArchiveDelegate() {}

    virtual bool open(QnResourcePtr resource) = 0;
    virtual void close() = 0;
    virtual qint64 startTime() = 0;
    virtual qint64 endTime() = 0;
    virtual QnAbstractMediaDataPtr getNextData() = 0;
    // If findIFrame=true, jump to position before time to a nearest IFrame.
    virtual qint64 seek (qint64 time, bool findIFrame) = 0;
    virtual QnVideoResourceLayout* getVideoLayout() = 0;
    virtual QnResourceAudioLayout* getAudioLayout() = 0;

    virtual AVCodecContext* setAudioChannel(int /*num*/) { return 0; }
    
    // this call inform delegate that reverse mode on/off
    virtual void onReverseMode(qint64 /*displayTime*/, bool /*value*/) {}

    // for optimization. Inform delegate to pause after sending 1 frame
    virtual void setSingleshotMode(bool /*value*/) {}

    // MediaStreamQuality. By default, this function is not implemented. Return: true if need seek for change quality
    virtual bool setQuality(MediaQuality /*quality*/, bool /*fastSwitch*/) { return false; }

    Flags getFlags() const { return m_flags; }
    virtual bool isRealTimeSource() const { return false; }
    virtual void beforeClose() {}

    /** This function calls from reader */
    virtual void beforeSeek(qint64 /*time*/) {}

    /** This function calls from reader */
    virtual void beforeChangeReverseMode(bool /*reverseMode*/) {}

protected:
    Flags m_flags;
};

#endif
