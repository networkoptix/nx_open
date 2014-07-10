/**********************************************************
* 5 sep 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_ARCHIVE_DELEGATE_H
#define THIRD_PARTY_ARCHIVE_DELEGATE_H

#ifdef ENABLE_THIRD_PARTY

#include <plugins/camera_plugin.h>
#include <plugins/resource/archive/abstract_archive_delegate.h>


class ThirdPartyArchiveDelegate
:
    public QnAbstractArchiveDelegate
{
public:
    ThirdPartyArchiveDelegate(
        const QnResourcePtr& resource,
        nxcip::DtsArchiveReader* archiveReader );
    virtual ~ThirdPartyArchiveDelegate();

    //!Implementation of QnAbstractArchiveDelegate::open
    virtual bool open(const QnResourcePtr &resource ) override;
    //!Implementation of QnAbstractArchiveDelegate::close
    virtual void close() override;
    //!Implementation of QnAbstractArchiveDelegate::startTime
    virtual qint64 startTime() override;
    //!Implementation of QnAbstractArchiveDelegate::endTime
    virtual qint64 endTime() override;
    //!Implementation of QnAbstractArchiveDelegate::getNextData
    virtual QnAbstractMediaDataPtr getNextData() override;
    //!Implementation of QnAbstractArchiveDelegate::seek
    virtual qint64 seek( qint64 time, bool findIFrame ) override;
    //!Implementation of QnAbstractArchiveDelegate::getVideoLayout
    virtual QnResourceVideoLayoutPtr getVideoLayout() override;
    //!Implementation of QnAbstractArchiveDelegate::getAudioLayout
    virtual QnResourceAudioLayoutPtr getAudioLayout() override;
    //!Implementation of QnAbstractArchiveDelegate::onReverseMode
    virtual void onReverseMode( qint64 displayTime, bool value ) override;
    //!Implementation of QnAbstractArchiveDelegate::setSingleshotMode
    virtual void setSingleshotMode( bool value ) override;
    //!Implementation of QnAbstractArchiveDelegate::setQuality
    virtual bool setQuality( MediaQuality quality, bool fastSwitch ) override;
    //!Implementation of QnAbstractArchiveDelegate::setRange
    virtual void setRange( qint64 startTime, qint64 endTime, qint64 frameStep ) override;
    //!Implementation of QnAbstractArchiveDelegate::setSendMotion
    virtual void setSendMotion( bool value ) override;

private:
    QnResourcePtr m_resource;
    nxcip::DtsArchiveReader* m_archiveReader;
    nxcip::StreamReader* m_streamReader;
    QnAbstractMediaDataPtr m_savedMediaPacket;
    unsigned int m_cSeq;
};

#endif // ENABLE_THIRD_PARTY

#endif  //THIRD_PARTY_ARCHIVE_DELEGATE_H
