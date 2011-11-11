#ifndef __ABSTRACT_ARCHIVE_DELEGATE_H
#define __ABSTRACT_ARCHIVE_DELEGATE_H

#include <QObject>
#include <QVector>
#include "core/resource/resource.h"
#include "core/datapacket/mediadatapacket.h"

class QnVideoResourceLayout;
class QnResourceAudioLayout;

class QnAbstractArchiveDelegate: public QObject
{
public:
    enum __Flags 
    { 
        Flag_SlowSource = 1, 
        Flag_CanProcessNegativeSpeed = 2 // flag inform that delegate is going to process negative speed. If flag is not setted, ArchiveReader is going to process negative speed
    };
    Q_DECLARE_FLAGS(Flags, __Flags);

    QnAbstractArchiveDelegate(): m_flags(0) {}
    virtual ~QnAbstractArchiveDelegate() {}

    virtual bool open(QnResourcePtr resource) = 0;
    virtual void close() = 0;
    virtual qint64 startTime() = 0;
    virtual qint64 endTime() = 0;
    virtual QnAbstractMediaDataPtr getNextData() = 0;
    virtual qint64 seek (qint64 time) = 0;
    virtual QnVideoResourceLayout* getVideoLayout() = 0;
    virtual QnResourceAudioLayout* getAudioLayout() = 0;

    virtual AVCodecContext* setAudioChannel(int /*num*/) { return 0; }
    
    // this call inform delegate that reverse mode on/off
    virtual void onReverseMode(qint64 /*displayTime*/, bool /*value*/) {}

    Flags getFlags() const { return m_flags; }
    virtual bool isRealTimeSource() const { return false; }
    virtual void beforeClose() {}
protected:
    Flags m_flags;
};

#endif
