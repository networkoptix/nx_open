#pragma once

#include <nx/streaming/abstract_archive_delegate.h>
#include <recording/time_period_list.h>

#if defined(ENABLE_HANWHA)

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaStreamReader;

class HanwhaNvrArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    HanwhaNvrArchiveDelegate(const QnResourcePtr& res);
    virtual ~HanwhaNvrArchiveDelegate();

    virtual bool open(const QnResourcePtr &resource) override;
    virtual void close() override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 timeUsec, bool findIFrame) override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

    virtual void beforeClose() override;

    virtual void onReverseMode(qint64 displayTime, bool value);
    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) override;

    virtual void setGroupId(const QByteArray& data) override;
    virtual void beforeSeek(qint64 time) override;
private:
    std::shared_ptr<HanwhaStreamReader> m_streamReader;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
