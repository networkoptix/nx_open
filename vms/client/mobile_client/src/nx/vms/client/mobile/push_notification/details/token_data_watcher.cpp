// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "token_data_watcher.h"

namespace nx::vms::client::mobile::details {

TokenDataWatcher& TokenDataWatcher::instance()
{
    static TokenDataWatcher watcher;
    return watcher;
}

TokenData TokenDataWatcher::data() const
{
    return m_tokenData.value_or(TokenData::makeEmpty());
}

void TokenDataWatcher::setData(const TokenData& value)
{
    if (m_tokenData.has_value() && *m_tokenData == value)
        return;

    m_tokenData = value;
    emit tokenDataChanged();
}

void TokenDataWatcher::resetData()
{
    if (!m_tokenData.has_value())
        return;

    m_tokenData.reset();
    emit tokenDataChanged();
}

bool TokenDataWatcher::hasData() const
{
    return m_tokenData.has_value();
}

} // namespace nx::vms::client::mobile::details
