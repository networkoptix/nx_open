#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/desktop/common/models/abstract_mapping_model.h>

namespace nx::vms::client::desktop {

/**
 * This mapping model allows to represent an expanded part of a tree model as a list model.
 *
 * Column 0 is used at all levels of the tree and must exist in the source model.
 *
 * Expand/collapse, source row insertion and removal operations have O(n) complexity, where n is
 * this model row count.
 *
 * This model introduces new data roles: LevelRole, HasChildrenRole, ExpandedRole.
 * The first two are read-only, the last one can be used in setData to expand/collapse nodes.
 */
class NX_VMS_CLIENT_DESKTOP_API LinearizationListModel:
    public ScopedModelOperations<AbstractMappingModel<QAbstractListModel>>
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* sourceModel READ sourceModel WRITE setSourceModel
        NOTIFY sourceModelChanged)

    using base_type = ScopedModelOperations<AbstractMappingModel<QAbstractListModel>>;

public:
    LinearizationListModel(QObject* parent = nullptr);
    virtual ~LinearizationListModel() override;

    virtual QAbstractItemModel* sourceModel() const override;
    void setSourceModel(QAbstractItemModel* value); //< Does not take ownership.

    Q_INVOKABLE virtual QModelIndex mapToSource(const QModelIndex& index) const override;
    Q_INVOKABLE virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(
        const QModelIndex& index, const QVariant& value, int role = Qt::EditRole)  override;

    virtual QHash<int, QByteArray> roleNames() const override;

    void expandAll();
    void collapseAll();

    static void registerQmlType();

    enum Roles
    {
        LevelRole = Qt::UserRole + 10000,
        HasChildrenRole,
        ExpandedRole
    };
    Q_ENUM(Roles)

signals:
    void sourceModelChanged(QPrivateSignal);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
