// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QStringList>

#include <nx/vms/client/desktop/event_search/models/abstract_async_search_list_model.h>

namespace nx::vms::client::desktop {

class VmsEventSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

    Q_PROPERTY(QString selectedEventType READ selectedEventType
        WRITE setSelectedEventType NOTIFY selectedEventTypeChanged)

    Q_PROPERTY(QString selectedSubType READ selectedSubType
        WRITE setSelectedSubType NOTIFY selectedSubTypeChanged)

public:
    explicit VmsEventSearchListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~VmsEventSearchListModel() override = default;

    QString selectedEventType() const;
    void setSelectedEventType(const QString& value);

    QString selectedSubType() const;
    void setSelectedSubType(const QString& value);

    // Event type list used when selectedEventType() == EventType::undefined.
    const QStringList& defaultEventTypes() const;
    void setDefaultEventTypes(const QStringList& value);

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

signals:
    void selectedEventTypeChanged();
    void selectedSubTypeChanged();

private:
    class Private;
    Private* const d;
};

} // namespace nx::vms::client::desktop
