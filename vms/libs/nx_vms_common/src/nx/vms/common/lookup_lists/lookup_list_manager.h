// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/lookup_list_data.h>

namespace nx::vms::common {

/**
 * Stores all Lookup Lists in the System. After initialization listens for the Message Processor
 * signals and maintains data in actual state. Can be initialized directly (server-side and tests
 * scenario) or using remote request (client-side scenario). Supports deinitialization (actual for
 * the Client while it has single System Context for different connections).
 */
class NX_VMS_COMMON_API LookupListManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit LookupListManager(QObject* parent = nullptr);
    virtual ~LookupListManager() override;

    bool isInitialized() const;
    void initialize(nx::vms::api::LookupListDataList initialData);
    void deinitialize();

    nx::vms::api::LookupListData lookupList(const QnUuid& id) const;
    const nx::vms::api::LookupListDataList& lookupLists() const;

    void addOrUpdate(nx::vms::api::LookupListData list);
    void remove(const QnUuid& id);
    QList<int> filter(const nx::vms::api::LookupListData& list,
        const QString& filterText,
        int resultLimit) const;

signals:
    void initialized();
    void addedOrUpdated(QnUuid id);
    void removed(QnUuid id);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
