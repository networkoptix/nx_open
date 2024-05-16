// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QStringList>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/event_search/models/abstract_async_search_list_model.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class WindowContext;

class VmsEventSearchListModel: public core::AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

    Q_PROPERTY(QString selectedEventType READ selectedEventType
        WRITE setSelectedEventType NOTIFY selectedEventTypeChanged)

    Q_PROPERTY(QString selectedSubType READ selectedSubType
        WRITE setSelectedSubType NOTIFY selectedSubTypeChanged)

public:
    explicit VmsEventSearchListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~VmsEventSearchListModel() override;

    QString selectedEventType() const;
    void setSelectedEventType(const QString& value);

    QString selectedSubType() const;
    void setSelectedSubType(const QString& value);

    // Event type list used when selectedEventType() == EventType::undefined.
    const QStringList& defaultEventTypes() const;
    void setDefaultEventTypes(const QStringList& value);

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

public:
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role) const override;

protected:
    virtual bool requestFetch(
        const core::FetchRequest& request,
        const FetchCompletionHandler& completionHandler) override;

    virtual void clearData() override;

signals:
    void selectedEventTypeChanged();
    void selectedSubTypeChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
