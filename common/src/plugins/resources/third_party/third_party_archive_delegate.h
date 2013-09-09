/**********************************************************
* 5 sep 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_ARCHIVE_DELEGATE_H
#define THIRD_PARTY_ARCHIVE_DELEGATE_H

#include <plugins/camera_plugin.h>
#include <plugins/resources/archive/abstract_archive_delegate.h>


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
    virtual bool open( QnResourcePtr resource ) override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual void close() override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual qint64 startTime() override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual qint64 endTime() override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual QnAbstractMediaDataPtr getNextData() override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual qint64 seek( qint64 time, bool findIFrame ) override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual QnResourceVideoLayout* getVideoLayout() override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual QnResourceAudioLayout* getAudioLayout() override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual void onReverseMode( qint64 displayTime, bool value ) override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual void setSingleshotMode( bool value ) override;
    //!Implementation of QnAbstractArchiveDelegate::open
    virtual bool setQuality( MediaQuality quality, bool fastSwitch ) override;
    //!Implementation of QnAbstractArchiveDelegate::setRange
    virtual void setRange( qint64 startTime, qint64 endTime, qint64 frameStep ) override;
    //!Implementation of QnAbstractArchiveDelegate::setSendMotion
    virtual void setSendMotion( bool value ) override;

private:
    QnResourcePtr m_resource;
    nxcip::DtsArchiveReader* m_archiveReader;
    nxcip::StreamReader* m_streamReader;
};

#endif  //THIRD_PARTY_ARCHIVE_DELEGATE_H
