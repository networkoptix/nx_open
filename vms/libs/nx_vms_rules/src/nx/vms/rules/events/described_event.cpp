// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "described_event.h"

#include "../utils/event_details.h"

namespace nx::vms::rules {

DescribedEvent::DescribedEvent(
    std::chrono::microseconds timestamp,
    const QString& caption,
    const QString& description)
    :
    BasicEvent(timestamp),
    m_caption(caption),
    m_description(description)
{
}

QString DescribedEvent::caption() const
{
    return m_caption;
}

void DescribedEvent::setCaption(const QString& caption)
{
    m_caption = caption;
}

QString DescribedEvent::description() const
{
    return m_description;
}

void DescribedEvent::setDescription(const QString& description)
{
    m_description = description;
}

QVariantMap DescribedEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, m_caption);
    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, m_description);
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());

    return result;
}

QString DescribedEvent::detailing() const
{
    QString eventDetailing;

    if (!m_caption.isEmpty())
        eventDetailing = m_caption;

    if (!m_description.isEmpty())
        eventDetailing += eventDetailing.isEmpty() ? m_description : ": " + m_description;

    return eventDetailing;
}

} // namespace nx::vms::rules
