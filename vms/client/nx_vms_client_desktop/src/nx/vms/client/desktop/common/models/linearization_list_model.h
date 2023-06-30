// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/core/common/models/abstract_mapping_model.h>

class QJSValue;

namespace nx::vms::client::desktop {

/**
 * This mapping model allows to represent an expanded part of a tree model as a list model.
 *
 * Column 0 is used at all levels of the tree and must exist in the source model.
 * Column operations (insertion, removal, move or sorting) on the source model are not allowed.
 *
 * Expand/collapse, source row insertion and removal operations have O(n) complexity, where n is
 * this model row count.
 *
 * This model introduces new data roles: LevelRole, HasChildrenRole, ExpandedRole.
 * The first two are read-only, the last one can be used in setData to expand/collapse nodes.
 */
class NX_VMS_CLIENT_DESKTOP_API LinearizationListModel:
    public ScopedModelOperations<core::AbstractMappingModel<QAbstractListModel>>
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* sourceModel READ sourceModel WRITE setSourceModel
        NOTIFY sourceModelChanged)
    Q_PROPERTY(QModelIndex sourceRoot READ sourceRoot WRITE setSourceRoot)
    Q_PROPERTY(QString autoExpandRoleName READ autoExpandRoleName WRITE setAutoExpandRoleName
        NOTIFY autoExpandRoleNameChanged)

    using base_type = ScopedModelOperations<core::AbstractMappingModel<QAbstractListModel>>;

public:
    LinearizationListModel(QObject* parent = nullptr);
    virtual ~LinearizationListModel() override;

    virtual QAbstractItemModel* sourceModel() const override;
    void setSourceModel(QAbstractItemModel* value); //< Does not take ownership.

    QModelIndex sourceRoot() const;
    void setSourceRoot(const QModelIndex& sourceIndex);

    QString autoExpandRoleName() const;
    void setAutoExpandRoleName(const QString& value);

    Q_INVOKABLE virtual QModelIndex mapToSource(const QModelIndex& index) const override;
    Q_INVOKABLE virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(
        const QModelIndex& index, const QVariant& value, int role = Qt::EditRole)  override;

    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void expandAll();
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE bool setExpanded(const QModelIndex& index, bool value, bool recursively);
    Q_INVOKABLE bool setSourceExpanded(const QModelIndex& sourceIndex, bool value);
    Q_INVOKABLE bool isSourceExpanded(const QModelIndex& sourceIndex) const;
    Q_INVOKABLE QModelIndexList expandedState() const; //< Returns source index list.
    Q_INVOKABLE void setExpandedState(const QModelIndexList& sourceIndices);
    Q_INVOKABLE void ensureVisible(const QModelIndexList& sourceIndices);
    /*
     * Traverse all top level rows of source model in `topLevelRows` recursively and expand
     * each index when `expandCallback(index)` returns `true`.
     */
    Q_INVOKABLE void setExpanded(const QJSValue& expandCallback, const QJSValue& topLevelRows);

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
    void autoExpandRoleNameChanged(QPrivateSignal);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
