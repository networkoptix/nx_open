// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <api/model/manual_camera_seach_reply.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/api/data/device_model.h>

namespace nx::vms::client::desktop {

class FoundDevicesModel: public ScopedModelOperations<QAbstractListModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    enum PresentedState
    {
        notPresentedState,
        addingInProgressState,
        alreadyAddedState
    };

    enum Roles
    {
        dataRole = Qt::UserRole + 1,
        presentedStateRole
    };

    enum Columns
    {
        brandColumn,
        modelColumn,
        addressColumn,
        presentedStateColumn,
        checkboxColumn,

        count
    };

    FoundDevicesModel(QObject* parent = nullptr);

    void addDevices(const std::vector<api::DeviceModelForSearch>& devices);
    void removeDevices(QStringList ids);
    int deviceCount(PresentedState presentedState) const;
    int deviceCount(PresentedState presentedState, bool isChecked) const;

    QModelIndex indexByPhysicalId(const QString& physicalId, int column = 0);

public:
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role) override;

    virtual QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;

    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    bool isCorrectRow(const QModelIndex& index) const;

    api::DeviceModelForSearch device(const QModelIndex& index) const;

    QVariant getDisplayData(const QModelIndex& index) const;
    QVariant getCheckStateData(const QModelIndex& index) const;
    QVariant getForegroundColorData(const QModelIndex& index) const;

    using PresentedStateWithCheckState = QPair<PresentedState, bool>;
    void incrementDeviceCount(const PresentedStateWithCheckState& deviceState);
    void decrementDeviceCount(const PresentedStateWithCheckState& deviceState);

private:
    using DeviceRowStateHash = QHash<QString, PresentedStateWithCheckState>;
    using DeviceCountHash = QHash<PresentedStateWithCheckState, int>;

    std::vector<api::DeviceModelForSearch> m_devices;
    DeviceRowStateHash m_deviceRowState;
    DeviceCountHash m_deviceCount;
};

} // namespace nx::vms::client::desktop
