// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_content_watcher.h"

namespace nx::vms::client::mobile {
namespace timeline {

std::optional<bool> AbstractContentWatcher::hasContent() const
{
    return m_forced ? std::optional<bool>(true) : m_hasContent;
}

void AbstractContentWatcher::setHasContent(std::optional<bool> value)
{
    if (m_hasContent == value)
        return;

    m_hasContent = value;
    emit hasContentChanged();
}

void AbstractContentWatcher::setForced(bool value)
{
    if (m_forced == value)
        return;

    m_forced = value;
    emit hasContentChanged();
}

bool AbstractContentWatcher::isForced() const
{
    return m_forced;
}

} // namespace timeline
} // namespace nx::vms::client::mobile
