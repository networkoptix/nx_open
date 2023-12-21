// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <QtCore/QAbstractTableModel>

#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::client::desktop::rules {

class NX_VMS_CLIENT_DESKTOP_API RulesTableModel:
    public QAbstractTableModel,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT

public:
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
        FieldRole,
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

    /** Returns list of the rule ids for the given indexes. */
    QnUuidList getRuleIds(const QModelIndexList& indexes) const;

    static void registerQmlType();

private:
    using ConstRulePtr = std::shared_ptr<const vms::rules::Rule>;

    vms::rules::Engine* m_engine{nullptr};
    std::vector<QnUuid> m_ruleIds;

    void onRuleAddedOrUpdated(QnUuid ruleId, bool added);
    void onRuleRemoved(QnUuid ruleId);
    void onRulesReset();

    void initialise();
    bool isIndexValid(const QModelIndex &index) const;
    bool isRuleValid(const ConstRulePtr& rule) const;

    QVariant stateColumnData(const ConstRulePtr& rule, int role) const;
    QVariant eventColumnData(const ConstRulePtr& rule, int role) const;

    QVariant sourceColumnData(const ConstRulePtr& rule, int role) const;
    QVariant sourceCameraData(const vms::rules::EventFilter* eventFilter, int role) const;
    QVariant sourceServerData(const vms::rules::EventFilter* eventFilter, int role) const;

    QVariant actionColumnData(const ConstRulePtr& rule, int role) const;

    QVariant targetColumnData(const ConstRulePtr& rule, int role) const;
    QVariant targetCameraData(const vms::rules::ActionBuilder* actionBuilder, int role) const;
    QVariant targetLayoutData(const vms::rules::ActionBuilder* actionBuilder, int role) const;
    QVariant targetUserData(const vms::rules::ActionBuilder* actionBuilder, int role) const;
    QVariant targetServerData(const vms::rules::ActionBuilder* actionBuilder, int role) const;

    QVariant systemData(int role) const;

    QVariant editedStateColumnData(const ConstRulePtr& rule, int role) const;
    QVariant enabledStateColumnData(const ConstRulePtr& rule, int role) const;
    QVariant commentColumnData(const ConstRulePtr& rule, int role) const;
};

} // namespace nx::vms::client::desktop::rules
