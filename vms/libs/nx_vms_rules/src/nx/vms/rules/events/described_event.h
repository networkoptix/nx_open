// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"

namespace nx::vms::rules {

/** Event class that has a caption and description properties. Must be used as a base class only. */
class NX_VMS_RULES_API DescribedEvent: public BasicEvent
{
    Q_OBJECT

    Q_PROPERTY(QString caption READ caption WRITE setCaption)
    Q_PROPERTY(QString description READ caption WRITE setDescription)

public:
    QString caption() const;
    void setCaption(const QString& caption);
    QString description() const;
    void setDescription(const QString& description);

    virtual QVariantMap details(common::SystemContext* context) const override;

protected:
    DescribedEvent() = default;
    DescribedEvent(
        std::chrono::microseconds timestamp,
        const QString& caption,
        const QString& description);

private:
    QString m_caption;
    QString m_description;

    QString detailing() const;
};

} // namespace nx::vms::rules
