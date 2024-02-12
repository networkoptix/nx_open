// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/showreel_data.h>

namespace nx::vms::common {

class NX_VMS_COMMON_API ShowreelManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;
public:
    ShowreelManager(QObject* parent = nullptr);
    virtual ~ShowreelManager() override;

    const nx::vms::api::ShowreelDataList& showreels() const;
    void resetShowreels(const nx::vms::api::ShowreelDataList& showreels = {});

    nx::vms::api::ShowreelData showreel(const nx::Uuid& id) const;
    nx::vms::api::ShowreelDataList showreels(const QList<nx::Uuid>& ids) const;

    void addOrUpdateShowreel(const nx::vms::api::ShowreelData& showreel);
    void removeShowreel(const nx::Uuid& showreelId);

signals:
    void showreelAdded(const nx::vms::api::ShowreelData& showreel);
    void showreelChanged(const nx::vms::api::ShowreelData& showreel);
    void showreelRemoved(const nx::Uuid& showreelId);

private:
    mutable nx::Mutex m_mutex;
    nx::vms::api::ShowreelDataList m_showreels;
};

} // namespace nx::vms::common
