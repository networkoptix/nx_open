// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>
#include <vector>

#include <QtCore/QAbstractTableModel>

#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::client::desktop::rules {

/** Facade around Rule class. Simplifies access to the rule properties. */
class NX_VMS_CLIENT_DESKTOP_API SimplifiedRule: public QObject
{
    Q_OBJECT

public:
    virtual ~SimplifiedRule() override;

    QnUuid id() const;

    QString eventType() const;
    void setEventType(const QString& eventType);
    vms::rules::EventFilter* eventFilter() const;
    QHash<QString, nx::vms::rules::Field*> eventFields() const;
    std::optional<nx::vms::rules::ItemDescriptor> eventDescriptor() const;

    QString actionType() const;
    vms::rules::ActionBuilder* actionBuilder() const;
    void setActionType(const QString& actionType);
    QHash<QString, nx::vms::rules::Field*> actionFields() const;
    std::optional<nx::vms::rules::ItemDescriptor> actionDescriptor() const;

    QString comment() const;
    void setComment(const QString& comment);

    bool enabled() const;
    void setEnabled(bool value);

    QByteArray schedule() const;
    void setSchedule(const QByteArray& schedule);

    /** Calls model updateRule() method with the stored through the setModelIndex() method index. */
    Q_INVOKABLE void update();

    QPersistentModelIndex modelIndex() const;

private:
    QPersistentModelIndex m_index;
    nx::vms::rules::Engine* m_engine = nullptr;
    std::unique_ptr<vms::rules::Rule> m_rule;

    /** Only RulesTableModel must has an ability to create SimplifiedRule instances. */
    friend class RulesTableModel;

    SimplifiedRule(vms::rules::Engine* engine, std::unique_ptr<vms::rules::Rule>&& rule);

    void setRule(std::unique_ptr<vms::rules::Rule>&& rule);
    const vms::rules::Rule* rule() const;
    void setModelIndex(const QPersistentModelIndex& modelIndex);
    void update(const QVector<int>& roles);

    /** Starts watching on the builder's and filter's changes of the rule set. */
    void startWatchOnRule() const;
    void startWatchOnEventFilter() const;
    void startWatchOnActionBuilder() const;

    /** Stops watching on the builder's and filter's changes of the rule set. */
    void stopWatchOnRule() const;
    void stopWatchOnEventFilter() const;
    void stopWatchOnActionBuilder() const;

    /** Connects all the notify signal of the object to the update() method. */
    void watchOn(QObject* object) const;
};

class NX_VMS_CLIENT_DESKTOP_API RulesTableModel:
    public QAbstractTableModel,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT

public:
    enum Columns
    {
        IdColumn,
        EventColumn,
        ActionColumn,
        CommentColumn,
        EnabledStateColumn,
        EditedStateColumn,
        ColumnsCount
    };

    enum Roles
    {
        FilterRole = Qt::UserRole,
        FieldRole
    };

    explicit RulesTableModel(nx::vms::rules::Engine* engine, QObject* parent = nullptr);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole) override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    /** Add new rule and return its index. Returns invalid index if rule was not added. */
    QModelIndex addRule(const QString& eventId, const QString& actionId);

    /** Remove rule under the index. Returns true if removed, false otherwise. */
    bool removeRule(const QModelIndex& ruleIndex);

    /** Remove rule under the id. Returns true if removed, false otherwise. */
    bool removeRule(const QnUuid& id);

    /** Returns simplified rule under the index. Returns nullptr if there is no such a rule. */
    std::weak_ptr<SimplifiedRule> rule(const QModelIndex& ruleIndex) const;

    /** Returns simplified rule under the id. Returns nullptr if there is no such a rule. */
    std::weak_ptr<SimplifiedRule> rule(const QnUuid& id) const;

    /** If the rule was edited required to call this method to notify the model. */
    void updateRule(const QModelIndex& ruleIndex, const QVector<int>& roles);

    bool hasChanges() const;
    void applyChanges(std::function<void(const QString&)> errorHandler = {});
    void rejectChanges();
    void resetToDefaults(std::function<void(const QString&)> errorHandler = {});

signals:
    void stateChanged();

private:
    std::vector<std::shared_ptr<SimplifiedRule>> m_simplifiedRules;
    nx::vms::rules::Engine* m_engine = nullptr;

    std::set<QnUuid> m_addedRules;
    std::set<QnUuid> m_modifiedRules;
    std::set<QnUuid> m_removedRules;

    void initialise();
    bool isIndexValid(const QModelIndex &index) const;
    bool isRuleModified(const SimplifiedRule* rule) const;

    void onRuleAddedOrUpdated(QnUuid ruleId, bool added);

    QVariant idColumnData(const QModelIndex& index, int role) const;
    QVariant eventColumnData(const QModelIndex& index, int role) const;
    QVariant actionColumnData(const QModelIndex& index, int role) const;
    QVariant editedStateColumnData(const QModelIndex& index, int role) const;
    QVariant enabledStateColumnData(const QModelIndex& index, int role) const;
    bool setEnabledStateColumnData(const QModelIndex& index, const QVariant& value, int role);
    QVariant commentColumnData(const QModelIndex& index, int role) const;
};

} // namespace nx::vms::client::desktop::rules
