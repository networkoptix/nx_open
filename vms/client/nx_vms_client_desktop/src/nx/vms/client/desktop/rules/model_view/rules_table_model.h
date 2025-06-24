// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QAbstractTableModel>

#include <core/resource/resource_fwd.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::client::desktop::rules {

class NX_VMS_CLIENT_DESKTOP_API RulesTableModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    using ConstRulePtr = vms::rules::ConstRulePtr;

    enum Columns
    {
        StateColumn,
        EventColumn,
        SourceColumn,
        ActionColumn,
        TargetColumn,
        CommentColumn,
        ColumnsCount
    };
    Q_ENUM(Columns)

    enum Roles
    {
        RuleIdRole = Qt::UserRole,
        ResourceIdsRole,
        IsSystemRuleRole,
        SortDataRole
    };
    Q_ENUM(Roles)

    explicit RulesTableModel(QObject* parent = nullptr);

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    ConstRulePtr getRule(const QModelIndex& index) const;

    static void registerQmlType();

    /**
     * The given id will be added to the bunch of the resource ids returned for the Resource Ids
     * Role if the `acceptAll` property of the `SourceCameraField` is set to `true`.
     */
    static const Uuid kAnyDeviceUuid;

private:
    vms::rules::Engine* m_engine{nullptr};
    std::vector<nx::Uuid> m_ruleIds;

    void onRuleAddedOrUpdated(nx::Uuid ruleId, bool added);
    void onRuleRemoved(nx::Uuid ruleId);
    void onRulesReset();
    void onResourcePoolChanged(const QnResourceList& resources);
    void onPermissionsChanged(const QnResourceList& resources);
    void onLookupListRemoved(nx::Uuid id);

    void initialise();
    bool isIndexValid(const QModelIndex& index) const;
    bool canDisplayRule(const ConstRulePtr& rule) const;

    QVariant stateColumnData(const ConstRulePtr& rule, int role) const;
    QVariant eventColumnData(const ConstRulePtr& rule, int role) const;

    QVariant sourceColumnData(const ConstRulePtr& rule, int role) const;
    QVariant sourceCameraData(const ConstRulePtr& rule, int role) const;
    QVariant sourceServerData(const ConstRulePtr& rule, int role) const;
    QVariant sourceUserData(const ConstRulePtr& rule, int role) const;

    QVariant actionColumnData(const ConstRulePtr& rule, int role) const;

    QVariant targetColumnData(const ConstRulePtr& rule, int role) const;
    QVariant targetCameraData(const ConstRulePtr& rule, int role) const;
    QVariant targetLayoutData(const ConstRulePtr& rule, int role) const;
    QVariant targetUserData(const ConstRulePtr& rule, int role) const;
    QVariant targetServerData(const ConstRulePtr& rule, int role) const;

    QVariant systemData(const ConstRulePtr& rule, int role) const;

    QVariant editedStateColumnData(const ConstRulePtr& rule, int role) const;
    QVariant enabledStateColumnData(const ConstRulePtr& rule, int role) const;
    QVariant commentColumnData(const ConstRulePtr& rule, int role) const;

    // Returns all the resource ids for the given row and column.
    QSet<nx::Uuid> resourceIds(int row, int column) const;

    QSet<nx::Uuid> sourceIds(
        const ConstRulePtr& rule,
        const vms::rules::ItemDescriptor& descriptor) const;
    QSet<nx::Uuid> targetIds(
        const ConstRulePtr& rule,
        const vms::rules::ItemDescriptor& descriptor) const;

    // Returns whether the rule under the given row contains source or target device from the
    // given resources list.
    bool hasAnyOf(int row, const QList<nx::Uuid>& resourceIds) const;

    // Returns whether the given the rule at the given row has resources.
    bool hasResources(int row);
};

} // namespace nx::vms::client::desktop::rules
