#pragma once

#include <QtCore/QAbstractListModel>

#include <common/common_module_aware.h>
#include <api/model/manual_camera_seach_reply.h>
#include <nx/utils/scoped_model_operations.h>

namespace nx {
namespace client {
namespace desktop {

class FoundDevicesModel:
    public ScopedModelOperations<QAbstractListModel>,
    public QnCommonModuleAware
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

    void addDevices(const QnManualResourceSearchList& devices);
    void removeDevices(QStringList ids);

    QModelIndex indexByUniqueId(const QString& uniqueId, int column = 0);

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

signals:
    void rowCountChanged();

private:
    bool correctIndex(const QModelIndex& index) const;

    QnManualResourceSearchEntry device(const QModelIndex& index) const;

    QVariant displayData(const QModelIndex& index) const;

    QVariant checkedData(const QModelIndex& index) const;

    int newDevicesCount() const;

    QPalette::ColorRole getColorRole(const QModelIndex& index) const;

private:
    using DeviceCheckedStateHash = QHash<QString, bool>;
    using PresentedStateHash = QHash<QString, PresentedState>;
    using ColumnDifference = QHash<int, bool>;
    using FieldsDifferenceHash = QHash<QString, ColumnDifference>;

    DeviceCheckedStateHash m_checked;
    PresentedStateHash m_presentedState;

    QnManualResourceSearchList m_devices;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::FoundDevicesModel::PresentedState)
