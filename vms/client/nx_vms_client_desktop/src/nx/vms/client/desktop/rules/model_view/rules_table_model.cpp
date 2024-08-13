// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_table_model.h"

#include <QtQml/QtQml>

#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_server_field.h>
#include <nx/vms/rules/action_builder_fields/target_single_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/rule.h>

namespace nx::vms::client::desktop::rules {

namespace {

QString iconPath(QnResourceIconCache::Key iconKey)
{
    return "image://resource/" + QString::number(iconKey);
}

QnVirtualCameraResourceList getActualCameras(const QnResourcePool* resourcePool)
{
    return resourcePool->getResources<QnVirtualCameraResource>().filtered(
        [](const QnVirtualCameraResourcePtr& resource)
        {
            return !resource->hasFlags(Qn::desktop_camera);
        });
}

bool isRuleValid(const vms::rules::ConstRulePtr& rule)
{
    return rule->isValid()
        && vms::rules::utils::visibleFieldsValidity(
            rule.get(), appContext()->currentSystemContext()).isValid();
}

constexpr auto kAttentionIconPath = "20x20/Solid/attention.svg?primary=yellow";
constexpr auto kDisabledRuleIconPath = "20x20/Outline/block.svg?primary=light10";
constexpr auto kInvalidRuleIconPath = "20x20/Solid/alert2.svg?primary=yellow";

} // namespace

RulesTableModel::RulesTableModel(QObject* parent):
    QAbstractTableModel(parent),
    m_engine(appContext()->currentSystemContext()->vmsRulesEngine())
{
    initialise();

    connect(
        m_engine,
        &vms::rules::Engine::ruleAddedOrUpdated,
        this,
        &RulesTableModel::onRuleAddedOrUpdated);
    connect(m_engine, &vms::rules::Engine::ruleRemoved, this, &RulesTableModel::onRuleRemoved);
    connect(m_engine, &vms::rules::Engine::rulesReset, this, &RulesTableModel::onRulesReset);
}

void RulesTableModel::onRuleAddedOrUpdated(nx::Uuid ruleId, bool added)
{
    NX_VERBOSE(this, "Rule %1 is %2", ruleId, (added ? "added" : "updated"));

    if (added)
    {
        beginInsertRows({}, m_ruleIds.size(), m_ruleIds.size());
        m_ruleIds.push_back(ruleId);
        endInsertRows();
    }
    else
    {
        const auto ruleIt = std::find(m_ruleIds.cbegin(), m_ruleIds.cend(), ruleId);
        if (ruleIt == m_ruleIds.cend())
            return;

        const auto distance = std::distance(m_ruleIds.cbegin(), ruleIt);
        emit dataChanged(index(distance, StateColumn), index(distance, CommentColumn));
    }
};

void RulesTableModel::onRuleRemoved(nx::Uuid ruleId)
{
    NX_VERBOSE(this, "Rule %1 is removed", ruleId);

    const auto ruleIt = std::find(m_ruleIds.cbegin(), m_ruleIds.cend(), ruleId);
    if (ruleIt == m_ruleIds.cend())
        return;

    const auto distance = std::distance(m_ruleIds.cbegin(), ruleIt);
    beginRemoveRows({}, distance, distance);

    m_ruleIds.erase(ruleIt);

    endRemoveRows();
}

void RulesTableModel::onRulesReset()
{
    NX_VERBOSE(this, "Rules were reset");

    beginResetModel();
    initialise();
    endResetModel();
}

int RulesTableModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_ruleIds.size();
}

int RulesTableModel::columnCount(const QModelIndex& /*parent*/) const
{
    return ColumnsCount;
}

QVariant RulesTableModel::data(const QModelIndex& index, int role) const
{
    if (!isIndexValid(index))
        return {};

    const auto rule = m_engine->rule(m_ruleIds[index.row()]);
    if (!canDisplayRule(rule))
        return {};

    static const auto getEffectiveRole =
        [] (int role, int column) -> int
        {
            if (role == SortDataRole && column != StateColumn)
                return Qt::DisplayRole;

            return role;
        };

    const auto effectiveRole = getEffectiveRole(role, index.column());

    if (effectiveRole == IsRuleValidRole)
        return isRuleValid(rule);

    if (effectiveRole == IsSystemRuleRole)
        return rule->isInternal();

    if (effectiveRole == RuleIdRole)
        return QVariant::fromValue(rule->id());

    switch (index.column())
    {
        case StateColumn:
            return stateColumnData(rule, effectiveRole);
        case EventColumn:
            return eventColumnData(rule, effectiveRole);
        case SourceColumn:
            return sourceColumnData(rule, effectiveRole);
        case ActionColumn:
            return actionColumnData(rule, effectiveRole);
        case TargetColumn:
            return targetColumnData(rule, effectiveRole);
        case CommentColumn:
            return commentColumnData(rule, effectiveRole);
    }

    return {};
}

Qt::ItemFlags RulesTableModel::flags(const QModelIndex& index) const
{
    return QAbstractTableModel::flags(index) | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

QVariant RulesTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::SizeHintRole && section == StateColumn)
            return client::core::kIconSize;

        if (role == Qt::DisplayRole)
        {
            switch (section)
            {
                case StateColumn:
                    return "";
                case EventColumn:
                    return tr("Event");
                case SourceColumn:
                    return tr("Source");
                case ActionColumn:
                    return tr("Action");
                case TargetColumn:
                    return tr("Target");
                case CommentColumn:
                    return tr("Comment");
                default:
                    return {};
            }
        }
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

QHash<int, QByteArray> RulesTableModel::roleNames() const
{
    auto roles = QAbstractTableModel::roleNames();

    roles[RuleIdRole] = "ruleId";
    roles[IsRuleValidRole] = "isValid";
    roles[SortDataRole] = "sortData";

    return roles;
}

void RulesTableModel::initialise()
{
    const auto rules = m_engine->rules();

    m_ruleIds.clear();
    m_ruleIds.reserve(rules.size());

    for (const auto& rule: rules)
        m_ruleIds.push_back(rule.first);
}

bool RulesTableModel::isIndexValid(const QModelIndex& index) const
{
    return index.isValid() && index.row() < m_ruleIds.size();
}

bool RulesTableModel::canDisplayRule(const ConstRulePtr& rule) const
{
    if (!rule)
        return false;

    if (rule->eventFilters().empty() || rule->actionBuilders().empty())
        return false;

    if (!m_engine->eventDescriptor(rule->eventFilters().first()->eventType()).has_value())
        return false;

    if (!m_engine->actionDescriptor(rule->actionBuilders().first()->actionType()).has_value())
        return false;

    return true;
}

QVariant RulesTableModel::stateColumnData(const ConstRulePtr& rule, int role) const
{
    if (role == SortDataRole)
    {
        enum SortWeight
        {
            disabled,
            invalid,
            acceptable
        };

        if (!rule->enabled())
            return SortWeight::disabled;

        if (!isRuleValid(rule))
            return SortWeight::invalid;

        return SortWeight::acceptable;
    }

    if (role == Qt::CheckStateRole)
        return rule->enabled() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;

    if (role == Qt::DecorationRole)
    {
        if (!rule->enabled())
            return kDisabledRuleIconPath;

        if (!isRuleValid(rule))
            return kInvalidRuleIconPath;
    }

    return {};
}

QVariant RulesTableModel::eventColumnData(const ConstRulePtr& rule, int role) const
{
    if (role == Qt::DisplayRole)
        return m_engine->eventDescriptor(rule->eventFilters().first()->eventType())->displayName();

    return {};
}

QVariant RulesTableModel::sourceColumnData(const ConstRulePtr& rule, int role) const
{
    const auto eventFilter = rule->eventFilters().first();
    const auto eventDescriptor = m_engine->eventDescriptor(eventFilter->eventType());

    QVariant result;
    if (vms::rules::hasSourceCamera(eventDescriptor.value()))
        result = sourceCameraData(eventFilter, role);

    if (result.isNull() && vms::rules::hasSourceServer(eventDescriptor.value()))
        result = sourceServerData(eventFilter, role);

    if (result.isNull())
        result = systemData(role);

    return result;
}

QVariant RulesTableModel::sourceCameraData(const vms::rules::EventFilter* eventFilter, int role) const
{
    auto sourceCameraField = eventFilter->fieldByType<vms::rules::SourceCameraField>();
    if (sourceCameraField == nullptr)
        return {};

    const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
    const auto resources =
        resourcePool->getResourcesByIds<QnVirtualCameraResource>(sourceCameraField->ids());
    const auto properties = sourceCameraField->properties();
    const bool isValidSelection = !resources.empty() || properties.allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return vms::rules::Strings::selectCamera(appContext()->currentSystemContext());

        if (sourceCameraField->acceptAll())
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool,
                tr("Any Device"),
                tr("Any Camera"));
        }

        if (resources.isEmpty())
            return tr("No source");

        if (resources.size() == 1)
        {
            return QnResourceDisplayInfo(resources.first()).toString(
                appContext()->localSettings()->resourceInfoLevel());
        }

        return QnDeviceDependentStrings::getNumericName(resourcePool, resources);
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        if (sourceCameraField->acceptAll() || resources.size() > 1)
            return iconPath(QnResourceIconCache::Cameras);

        if (resources.size() == 1)
            return iconPath(qnResIconCache->key(resources.first()));

        return iconPath(QnResourceIconCache::Camera);
    }

    if (role == ResourceIdsRole)
        return QVariant::fromValue(nx::utils::toQSet(resources.ids()));

    return {};
}

QVariant RulesTableModel::sourceServerData(const vms::rules::EventFilter* eventFilter, int role) const
{
    auto sourceServerField = eventFilter->fieldByName<vms::rules::SourceServerField>(
        vms::rules::utils::kCameraIdFieldName);
    if (sourceServerField == nullptr)
        return {};

    const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
    const auto resources =
        resourcePool->getResourcesByIds<QnMediaServerResource>(sourceServerField->ids());
    const auto properties = sourceServerField->properties();
    const bool isValidSelection = !resources.empty() || properties.allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return vms::rules::Strings::selectServer();

        if (sourceServerField->acceptAll())
            return tr("Any Server");

        if (resources.isEmpty())
            return tr("No source");

        if (resources.size() == 1)
        {
            return QnResourceDisplayInfo(resources.first()).toString(
                appContext()->localSettings()->resourceInfoLevel());
        }

        return tr("%n Servers", "", static_cast<int>(resources.size()));
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        if (sourceServerField->acceptAll() || resources.size() > 1)
            return iconPath(QnResourceIconCache::Servers);

        if (resources.size() == 1)
            return iconPath(qnResIconCache->key(resources.first()));

        return iconPath(QnResourceIconCache::Server);
    }

    if (role == ResourceIdsRole)
        return QVariant::fromValue(nx::utils::toQSet(resources.ids()));

    return {};
}

QVariant RulesTableModel::actionColumnData(const ConstRulePtr& rule, int role) const
{
    if (role == Qt::DisplayRole)
        return m_engine->actionDescriptor(rule->actionBuilders().first()->actionType())->displayName();

    return {};
}

QVariant RulesTableModel::targetColumnData(const ConstRulePtr& rule, int role) const
{
    const auto actionBuilder = rule->actionBuilders().first();
    const auto actionDescriptor = m_engine->actionDescriptor(actionBuilder->actionType());

    QVariant result;
    if (vms::rules::hasTargetCamera(actionDescriptor.value()))
        result = targetCameraData(actionBuilder, role);

    if (result.isNull() && vms::rules::hasTargetLayout(actionDescriptor.value()))
        result = targetLayoutData(actionBuilder, role);

    if (result.isNull() && vms::rules::hasTargetUser(actionDescriptor.value()))
        result = targetUserData(actionBuilder, role);

    if (result.isNull() && vms::rules::hasTargetServer(actionDescriptor.value()))
        result = targetServerData(actionBuilder, role);

    if (result.isNull())
        result = systemData(role);

    return result;
}

QVariant RulesTableModel::targetCameraData(const vms::rules::ActionBuilder* actionBuilder, int role) const
{
    QnVirtualCameraResourceList resources;
    bool useSource{false};
    bool acceptAll{false};
    bool allowEmptySelection{false};
    const auto resourcePool = appContext()->currentSystemContext()->resourcePool();

    if (const auto targetDeviceField = actionBuilder->fieldByType<vms::rules::TargetDeviceField>())
    {
        useSource = targetDeviceField->useSource();
        allowEmptySelection = targetDeviceField->properties().allowEmptySelection;
        if (targetDeviceField->acceptAll())
        {
            acceptAll = true;
            resources = getActualCameras(resourcePool);
        }
        else
        {
            resources = resourcePool->getResourcesByIds<QnVirtualCameraResource>(
                targetDeviceField->ids());
        }
    }
    else if (const auto targetSingleDeviceField =
        actionBuilder->fieldByType<vms::rules::TargetSingleDeviceField>())
    {
        useSource = targetSingleDeviceField->useSource();
        allowEmptySelection = targetSingleDeviceField->properties().allowEmptySelection;
        resources = resourcePool->getResourcesByIds<QnVirtualCameraResource>(
            UuidSet{targetSingleDeviceField->id()});
    }
    else
    {
        return {};
    }

    const bool isValidSelection = useSource || !resources.isEmpty() || allowEmptySelection;
    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return vms::rules::Strings::selectCamera(appContext()->currentSystemContext());

        if (useSource)
        {
            return resources.empty()
                ? tr("Source camera")
                : tr("Source and %n more Cameras", "", static_cast<int>(resources.size()));
        }

        if (resources.isEmpty())
            return tr("No target");

        if (resources.size() == 1)
        {
            return QnResourceDisplayInfo(resources.first()).toString(
                appContext()->localSettings()->resourceInfoLevel());
        }

        return QnDeviceDependentStrings::getNumericName(resourcePool, resources);
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        const auto targetCamerasCount = resources.size() + (useSource ? 1 : 0);
        return (acceptAll || targetCamerasCount > 1)
            ? iconPath(QnResourceIconCache::Cameras)
            : iconPath(QnResourceIconCache::Camera);
    }

    if (role == ResourceIdsRole)
        return QVariant::fromValue(nx::utils::toQSet(resources.ids()));

    return {};
}

QVariant RulesTableModel::targetLayoutData(const vms::rules::ActionBuilder* actionBuilder, int role) const
{
    const auto targetLayoutField = actionBuilder->fieldByType<vms::rules::TargetLayoutField>();
    if (!targetLayoutField)
        return {};

    const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
    const auto layouts =
        resourcePool->getResourcesByIds<QnLayoutResource>(targetLayoutField->value());
    const auto properties = targetLayoutField->properties();
    const bool isValidSelection = !layouts.isEmpty() || properties.allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return tr("Select at least one layout");

        if (layouts.isEmpty())
            return tr("No target");

        if (layouts.size() == 1)
            return layouts.first()->getName();

        return tr("%n layouts", "", layouts.size());
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        return layouts.size() > 1
            ? iconPath(QnResourceIconCache::Layouts)
            : iconPath(QnResourceIconCache::Layout);
    }

    if (role == ResourceIdsRole)
        return QVariant::fromValue(nx::utils::toQSet(layouts.ids()));

    return {};
}

QVariant RulesTableModel::targetUserData(const vms::rules::ActionBuilder* actionBuilder, int role) const
{
    const auto targetUserField = actionBuilder->fieldByType<vms::rules::TargetUserField>();
    if (!targetUserField)
        return {};

    QnUserResourceList users;
    nx::vms::api::UserGroupDataList groups;
    nx::vms::common::getUsersAndGroups(
        appContext()->currentSystemContext(),
        targetUserField->ids(),
        users,
        groups);
    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

    const auto properties = targetUserField->properties();
    const bool isValidSelection = targetUserField->acceptAll()
        || !users.empty()
        || !groups.empty()
        || properties.allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return tr("Select at least one User");

        if (targetUserField->acceptAll())
            return tr("All Users");

        if (users.empty() && groups.empty())
            return tr("No target");

        if (users.size() == 1 && groups.empty())
            return users.front()->getName();

        if (users.empty() && groups.size() <= 2)
        {
            QStringList groupNames;
            for (const auto& group: groups)
                groupNames.push_back(std::move(group.name));

            groupNames.sort(Qt::CaseInsensitive);

            return groupNames.join(", ");
        }

        if (groups.empty())
            return tr("%n Users", "", users.size());

        if (!users.empty())
        {
            return QString{"%1, %2"}
                .arg(tr("%n Groups", "", groups.size()))
                .arg(tr("%n Users", "", users.size()));
        }

        return tr("%n Groups", "", groups.size());
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        return (targetUserField->acceptAll() || users.size() > 1 || !groups.empty())
            ? iconPath(QnResourceIconCache::Users)
            : iconPath(QnResourceIconCache::User);
    }

    if (role == ResourceIdsRole)
        return QVariant::fromValue(nx::utils::toQSet(users.ids()));

    return {};
}

QVariant RulesTableModel::targetServerData(
    const vms::rules::ActionBuilder* actionBuilder,
    int role) const
{
    const auto targetServerField = actionBuilder->fieldByType<vms::rules::TargetServerField>();
    if (!targetServerField)
        return {};

    const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
    const QnMediaServerResourceList targetServers =
        resourcePool->getResourcesByIds<QnMediaServerResource>(targetServerField->ids());
    const auto properties = targetServerField->properties();
    const bool isValidSelection = !targetServers.isEmpty() || properties.allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return vms::rules::Strings::selectServer();

        if (targetServerField->acceptAll())
            return tr("All Servers");

        if (targetServers.isEmpty())
            return tr("No target");

        const auto targetServersString = targetServers.size() > 1
            ? tr("%n Servers", "", targetServers.size())
            : targetServers.first()->getName();

        return targetServerField->useSource()
            ? tr("Source Server and %1").arg(targetServersString)
            : targetServersString;
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        const auto targetServerCount =
                targetServers.size() + (targetServerField->useSource() ? 1 : 0);
        return targetServerField->acceptAll() || targetServerCount > 1
            ? iconPath(QnResourceIconCache::Servers)
            : iconPath(QnResourceIconCache::Server);
    }

    return {};
}

QVariant RulesTableModel::systemData(int role) const
{
    if (role == Qt::DisplayRole)
        return tr("Site");

    if (role == Qt::DecorationRole)
        return iconPath(QnResourceIconCache::CurrentSystem);

    return {};
}

QVariant RulesTableModel::commentColumnData(const ConstRulePtr& rule, int role) const
{
    if (role == Qt::DisplayRole)
        return rule->comment();

    return {};
}

void RulesTableModel::registerQmlType()
{
    qmlRegisterType<rules::RulesTableModel>("nx.vms.client.desktop", 1, 0, "RulesTableModel");
}

} // namespace nx::vms::client::desktop::rules
