// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_data_provider_wrapper.h"

#include "audio_video_win/windows_desktop_resource.h"
#include "audio_video_win/desktop_data_provider.h"

namespace nx::vms::client::desktop {

DesktopDataProviderWrapper::DesktopDataProviderWrapper(
    QnResourcePtr res,
    core::DesktopDataProviderBase* owner)
    :
    QnAbstractMediaStreamDataProvider(res),
    m_owner(owner)
{
}

DesktopDataProviderWrapper::~DesktopDataProviderWrapper()
{
    m_owner->beforeDestroyDataProvider(this);
}

bool DesktopDataProviderWrapper::canAcceptData() const
{
    auto dataProcessors = m_dataprocessors.lock();
    for (int i = 0; i < dataProcessors->size(); ++i)
    {
        QnAbstractMediaDataReceptor* dp = dataProcessors->at(i);
        if (!dp->canAcceptData())
            return false;
    }

    return true;
}

bool DesktopDataProviderWrapper::needConfigureProvider() const
{
    const auto dataProcessors = m_dataprocessors.lock();
    for (const auto& dataProcessor: *dataProcessors)
    {
        if (dataProcessor->needConfigureProvider())
            return true;
    }
    return false;
}

void DesktopDataProviderWrapper::putData(const QnAbstractDataPacketPtr& data)
{
    const auto media = dynamic_cast<QnAbstractMediaData*>(data.get());
    if (!media)
        return;

    const auto dataProcessors = m_dataprocessors.lock();
    for (auto dp: *dataProcessors)
    {
        if (dp->canAcceptData())
        {
            if (media->dataType == QnAbstractMediaData::VIDEO)
            {
                auto itr = m_needKeyData.find(dp);
                if (itr != m_needKeyData.end())
                {
                    if (media->flags | AV_PKT_FLAG_KEY)
                        m_needKeyData.erase(itr);
                    else
                        continue; // skip data
                }
            }
            dp->putData(data);
        }
        else
        {
            m_needKeyData << dp;
        }
    }
}

void DesktopDataProviderWrapper::start(Priority /*priority*/)
{
    m_owner->start();
}

bool DesktopDataProviderWrapper::isInitialized() const
{
    return m_owner->isInitialized();
}

QString DesktopDataProviderWrapper::lastErrorStr() const
{
    return m_owner->lastErrorStr();
}

} // namespace nx::vms::client::desktop
