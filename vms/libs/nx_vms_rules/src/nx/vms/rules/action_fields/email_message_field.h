// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QByteArray>

#include <nx/vms/common/system_context_aware.h>

#include "text_with_fields.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API EmailMessageField:
    public ActionField,
    public common::SystemContextAware
{
    Q_OBJECT

    Q_CLASSINFO("metatype", "nx.actions.fields.emailMessageField")

    Q_PROPERTY(QString caption READ caption WRITE setCaption NOTIFY captionChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)

public:
    EmailMessageField(common::SystemContext* context);

    QString caption() const;
    void setCaption(const QString& caption);
    QString description() const;
    void setDescription(const QString& description);

    virtual QVariant build(const EventPtr& event) const override;

signals:
    void captionChanged();
    void descriptionChanged();

protected:
    TextWithFields m_caption;
    TextWithFields m_description;
};

} // namespace nx::vms::rules
