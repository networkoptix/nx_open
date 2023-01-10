// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/abstract_archive_delegate.h>

/*
* QnThumbnailsStreamReader gets frame sequence in the selected range with defined step.
*/

class NX_VMS_COMMON_API QnThumbnailsStreamReader: public QnAbstractMediaStreamDataProvider
{
public:
    QnThumbnailsStreamReader(
        const QnResourcePtr& resource,
        QnAbstractArchiveDelegate* archiveDelegate);
    virtual ~QnThumbnailsStreamReader();

    /*
    * Frame step in mks
    */
    void setRange(qint64 startTime, qint64 endTime, qint64 frameStep, int cseq);

    void setQuality(MediaQuality q);
    void setGroupId(const nx::String& groupId);
protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void run() override;
    virtual void afterRun() override;
private:
    QnAbstractMediaDataPtr createEmptyPacket();
private:
    QnAbstractArchiveDelegate* m_archiveDelegate;
    QnAbstractArchiveDelegate* m_delegate;
    std::atomic<int> m_cseq = 0;
};
