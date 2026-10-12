// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_table_model.h"

#include <vector>

#include <QtCore/QHash>
#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/rules/utils/strings.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_devices_field.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_layouts_field.h>
#include <nx/vms/rules/action_builder_fields/target_servers_field.h>
#include <nx/vms/rules/action_builder_fields/target_users_field.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/object_lookup_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>
#include <nx/vms/rules/event_filter_fields/text_lookup_field.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/resource.h>
#include <nx/vms/rules/utils/rule.h>

namespace nx::vms::client::desktop::rules {

namespace {

constexpr auto kAttentionIconPath = "20x20/Solid/attention.svg?primary=yellow";

constexpr auto kEnabledForegroundColor = "light10";
constexpr auto kDisabledForegroundColor = "dark16";

QString iconPath(QnResourceIconCache::Key iconKey, bool enabled)
{
    return QString{"image://resource/%1?primary=%2"}
        .arg(QString::number(iconKey))
        .arg(enabled ? kEnabledForegroundColor : kDisabledForegroundColor);
}

// Returns whether the given rule has Acceptable or Intermediate state.
bool calculateRuleValidity(const RulesTableModel::ConstRulePtr& rule)
{
    if (!rule->isCompatible())
        return false;

    const auto validity =
        vms::rules::utils::visibleFieldsValidity(rule.get(), rule->engine()->systemContext());

    return validity.validity > QValidator::State::Invalid;
}

bool hasLookupList(const vms::rules::ItemDescriptor& eventDescriptor)
{
    static const auto kTextLookupFieldType =
        vms::rules::utils::type<vms::rules::TextLookupField>();
    static const auto kObjectLookupFieldType =
        vms::rules::utils::type<vms::rules::ObjectLookupField>();

    return std::ranges::any_of(
        eventDescriptor.fields,
        [](const vms::rules::FieldDescriptor& fieldDescriptor)
        {
            return fieldDescriptor.type == kTextLookupFieldType
                || fieldDescriptor.type == kObjectLookupFieldType;
        });
}

/**
 * Cache of rule data that is expensive to calculate. The whole record is calculated on the first
 * access and is dropped when the rule or anything it depends on changes.
 */
class RuleDataCache
{
public:
    struct Data
    {
        bool isValid = false;
    };

    /**
     * Returns the rule data, calculating it on the first access. The reference is valid only
     * until the next call.
     */
    const Data& data(const RulesTableModel::ConstRulePtr& rule)
    {
        auto it = m_data.find(rule->id());
        if (it == m_data.end())
            it = m_data.insert(rule->id(), calculate(rule));

        return *it;
    }

    void invalidate(nx::Uuid ruleId) { m_data.remove(ruleId); }

    void clear() { m_data.clear(); }

private:
    Data calculate(const RulesTableModel::ConstRulePtr& rule) const
    {
        return {.isValid = calculateRuleValidity(rule)};
    }

private:
    QHash<nx::Uuid, Data> m_data;
};

} // namespace

struct RulesTableModel::Private
{
    vms::rules::Engine* const engine = nullptr;
    std::vector<nx::Uuid> ruleIds;

    /** Filled on demand from the const model methods. */
    mutable RuleDataCache ruleDataCache;

    /**
     * While inactive, data refresh signals are ignored. The model starts active, as the data
     * read before the first activation is up to date.
     */
    bool active = true;

    /** Returns whether the given rule has Acceptable or Intermediate state. */
    bool isRuleValid(const ConstRulePtr& rule) const { return ruleDataCache.data(rule).isValid; }
};

const Uuid RulesTableModel::kAnyDeviceUuid;

RulesTableModel::RulesTableModel(SystemContext* systemContext, QObject* parent):
    QAbstractTableModel(parent),
    SystemContextAware(systemContext),
    d(new Private{.engine = systemContext->vmsRulesEngine()})
{
    initialise();

    connect(d->engine,
        &vms::rules::Engine::ruleAddedOrUpdated,
        this,
        &RulesTableModel::onRuleAddedOrUpdated);
    connect(d->engine, &vms::rules::Engine::ruleRemoved, this, &RulesTableModel::onRuleRemoved);
    connect(d->engine, &vms::rules::Engine::rulesReset, this, &RulesTableModel::onRulesReset);

    connect(resourcePool(),
        &QnResourcePool::resourcesAdded,
        this,
        &RulesTableModel::onResourcePoolChanged);
    connect(resourcePool(),
        &QnResourcePool::resourcesRemoved,
        this,
        &RulesTableModel::onResourcePoolChanged);

    connect(accessController(),
        &core::AccessController::permissionsMaybeChanged,
        this,
        &RulesTableModel::onPermissionsChanged);

    connect(lookupListManager(),
        &common::LookupListManager::initialized,
        this,
        &RulesTableModel::onLookupListsInitialized);
    connect(lookupListManager(),
        &common::LookupListManager::removed,
        this,
        &RulesTableModel::onLookupListRemoved);
}

RulesTableModel::~RulesTableModel() = default;

void RulesTableModel::onRuleAddedOrUpdated(nx::Uuid ruleId, bool added)
{
    NX_VERBOSE(this, "Rule %1 is %2", ruleId, (added ? "added" : "updated"));

    if (added)
    {
        beginInsertRows({}, d->ruleIds.size(), d->ruleIds.size());
        d->ruleIds.push_back(ruleId);
        endInsertRows();
    }
    else
    {
        d->ruleDataCache.invalidate(ruleId);

        if (!d->active)
            return;

        const auto ruleIt = std::find(d->ruleIds.cbegin(), d->ruleIds.cend(), ruleId);
        if (ruleIt == d->ruleIds.cend())
            return;

        const auto distance = std::distance(d->ruleIds.cbegin(), ruleIt);
        emit dataChanged(index(distance, StateColumn), index(distance, CommentColumn));
    }
};

void RulesTableModel::onRuleRemoved(nx::Uuid ruleId)
{
    NX_VERBOSE(this, "Rule %1 is removed", ruleId);

    d->ruleDataCache.invalidate(ruleId);

    const auto ruleIt = std::find(d->ruleIds.cbegin(), d->ruleIds.cend(), ruleId);
    if (ruleIt == d->ruleIds.cend())
        return;

    const auto distance = std::distance(d->ruleIds.cbegin(), ruleIt);
    beginRemoveRows({}, distance, distance);

    d->ruleIds.erase(ruleIt);

    endRemoveRows();
}

void RulesTableModel::onRulesReset()
{
    NX_VERBOSE(this, "Rules were reset");

    beginResetModel();
    initialise();
    d->ruleDataCache.clear();
    endResetModel();
}

void RulesTableModel::onResourcePoolChanged(const QnResourceList& resources)
{
    if (!d->active)
        return;

    const auto resourceIds = resources.ids();

    for (int row = 0; row < rowCount({}); ++row)
    {
        if (hasAnyOf(row, resourceIds))
            refreshRow(row);
    }
}

void RulesTableModel::onPermissionsChanged(const QnResourceList& resources)
{
    if (!d->active)
        return;

    const auto resourceIds = resources.ids();

    for (int row = 0; row < rowCount({}); ++row)
    {
        const auto rule = d->engine->rule(d->ruleIds[row]);
        const auto eventFilter = rule->eventFilters().first();
        const auto eventDescriptor = d->engine->eventDescriptor(eventFilter->eventType());
        const auto actionBuilder = rule->actionBuilders().first();
        const auto actionDescriptor = d->engine->actionDescriptor(actionBuilder->actionType());

        if (!vms::rules::hasSourceUser(eventDescriptor.value())
            && !vms::rules::hasTargetUser(actionDescriptor.value()))
        {
            continue;
        }

        if (resourceIds.isEmpty())
        {
            // Permissions for any resource might be changed. Any rule with at least one selected
            // resource might be affected.
            if (hasResources(row))
                refreshRow(row);
        }
        else
        {
            if (hasAnyOf(row, resourceIds))
                refreshRow(row);
        }
    }
}

void RulesTableModel::onLookupListsInitialized()
{
    // All the lists are replaced, so any rule referencing a list might be affected.
    refreshLookupListRows(nx::Uuid());
}

void RulesTableModel::onLookupListRemoved(nx::Uuid id)
{
    refreshLookupListRows(id);
}

void RulesTableModel::refreshLookupListRows(nx::Uuid listId)
{
    if (!d->active)
        return;

    const auto predicate = [listId](auto field)
    {
        const Uuid fieldListId{field->value()};
        return listId.isNull() ? !fieldListId.isNull() : fieldListId == listId;
    };

    for (int row = 0; row < rowCount({}); ++row)
    {
        const auto rule = d->engine->rule(d->ruleIds[row]);
        const auto eventFilter = rule->eventFilters().first();
        const auto eventDescriptor = d->engine->eventDescriptor(eventFilter->eventType());

        if (!hasLookupList(eventDescriptor.value()))
            continue;

        const auto objectLookupFields = eventFilter->fieldsByType<vms::rules::ObjectLookupField>();
        if (std::ranges::any_of(objectLookupFields, predicate))
        {
            refreshRow(row);
            continue;
        }

        const auto textLookupFields = eventFilter->fieldsByType<vms::rules::TextLookupField>();
        if (std::ranges::any_of(textLookupFields.begin(), textLookupFields.end(), predicate))
            refreshRow(row);
    }
}

int RulesTableModel::rowCount(const QModelIndex& /*parent*/) const
{
    return d->ruleIds.size();
}

int RulesTableModel::columnCount(const QModelIndex& /*parent*/) const
{
    return ColumnsCount;
}

QVariant RulesTableModel::data(const QModelIndex& index, int role) const
{
    const auto rule = getRule(index);
    if (!rule)
        return {};

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

    if (effectiveRole == Qt::ForegroundRole)
    {
        return rule->enabled() && d->isRuleValid(rule)
            ? core::colorTheme()->color(kEnabledForegroundColor)
            : core::colorTheme()->color(kDisabledForegroundColor);
    }

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
                    return nx::vms::rules::Strings::source();
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

    roles[Qt::ForegroundRole] = "foregroundColor";

    roles[RuleIdRole] = "ruleId";
    roles[SortDataRole] = "sortData";

    return roles;
}

RulesTableModel::ConstRulePtr RulesTableModel::getRule(const QModelIndex& index) const
{
    if (!isIndexValid(index))
        return {};

    return d->engine->rule(d->ruleIds[index.row()]);
}

void RulesTableModel::setActive(bool active)
{
    if (d->active == active)
        return;

    d->active = active;

    if (!d->active)
        return;

    // Changes are not tracked while inactive, so everything must be refreshed on activation.
    d->ruleDataCache.clear();

    if (rowCount({}) > 0)
        emit dataChanged(createIndex(0, 0), createIndex(rowCount({}) - 1, ColumnsCount - 1));
}

void RulesTableModel::initialise()
{
    const auto rules = d->engine->rules();

    d->ruleIds.clear();
    d->ruleIds.reserve(rules.size());

    for (const auto& rule: rules)
        d->ruleIds.push_back(rule.first);
}

bool RulesTableModel::isIndexValid(const QModelIndex& index) const
{
    return index.isValid() && index.row() < d->ruleIds.size();
}

bool RulesTableModel::canDisplayRule(const ConstRulePtr& rule) const
{
    if (!rule)
        return false;

    if (rule->eventFilters().empty() || rule->actionBuilders().empty())
        return false;

    if (!d->engine->eventDescriptor(rule->eventFilters().first()->eventType()).has_value())
        return false;

    if (!d->engine->actionDescriptor(rule->actionBuilders().first()->actionType()).has_value())
        return false;

    return true;
}

void RulesTableModel::refreshRow(int row)
{
    d->ruleDataCache.invalidate(d->ruleIds[row]);
    emit dataChanged(createIndex(row, StateColumn), createIndex(row, CommentColumn));
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

        if (!d->isRuleValid(rule))
            return SortWeight::invalid;

        return SortWeight::acceptable;
    }

    if (role == Qt::CheckStateRole)
        return rule->enabled() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;

    if (role == Qt::DecorationRole)
    {
        if (!d->isRuleValid(rule))
            return kAttentionIconPath;
    }

    return {};
}

QVariant RulesTableModel::eventColumnData(const ConstRulePtr& rule, int role) const
{
    if (role == Qt::DisplayRole)
    {
        return d->engine->eventDescriptor(rule->eventFilters().first()->eventType())
            ->displayName();
    }

    return {};
}

QVariant RulesTableModel::sourceColumnData(const ConstRulePtr& rule, int role) const
{
    const auto eventFilter = rule->eventFilters().first();
    const auto eventDescriptor = d->engine->eventDescriptor(eventFilter->eventType());

    if (role == ResourceIdsRole)
        return QVariant::fromValue(sourceIds(rule, eventDescriptor.value()));

    QVariant result;
    if (vms::rules::hasSourceCamera(eventDescriptor.value()))
        result = sourceCameraData(rule, role);

    if (result.isNull() && vms::rules::hasSourceServer(eventDescriptor.value()))
        result = sourceServerData(rule, role);

    if (result.isNull())
        result = systemData(rule, role);

    return result;
}

QVariant RulesTableModel::sourceCameraData(const ConstRulePtr& rule, int role) const
{
    const auto eventFilter = rule->eventFilters().first();
    auto sourceCameraField = eventFilter->fieldByType<vms::rules::SourceCameraField>();
    if (sourceCameraField == nullptr)
        return {};

    auto ids = sourceCameraField->ids();
    if (role == ResourceIdsRole)
    {
        if (sourceCameraField->acceptAll())
            ids += kAnyDeviceUuid;

        return QVariant::fromValue(ids);
    }

    const auto resources = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(ids);
    const auto properties = sourceCameraField->properties();
    const bool isValidSelection = !resources.empty() || properties.allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return vms::rules::Strings::selectCamera(systemContext());

        if (sourceCameraField->acceptAll())
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(), tr("Any Device"), tr("Any Camera"));
        }

        if (resources.isEmpty())
            return tr("No source");

        if (resources.size() == 1)
        {
            return QnResourceDisplayInfo(resources.first()).toString(
                appContext()->localSettings()->resourceInfoLevel());
        }

        return QnDeviceDependentStrings::getNumericName(resourcePool(), resources);
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        if (sourceCameraField->acceptAll() || resources.size() > 1)
            return iconPath(QnResourceIconCache::Cameras, rule->enabled() && d->isRuleValid(rule));

        if (resources.size() == 1)
        {
            return iconPath(
                qnResIconCache->key(resources.first()), rule->enabled() && d->isRuleValid(rule));
        }

        return iconPath(QnResourceIconCache::Camera, rule->enabled() && d->isRuleValid(rule));
    }

    return {};
}

QVariant RulesTableModel::sourceServerData(const ConstRulePtr& rule, int role) const
{
    const auto eventFilter = rule->eventFilters().first();
    auto sourceServerField = eventFilter->fieldByName<vms::rules::SourceServerField>(
        vms::rules::utils::kServerIdFieldName);
    if (sourceServerField == nullptr)
        return {};

    const auto ids = sourceServerField->ids();
    if (role == ResourceIdsRole)
        return QVariant::fromValue(ids);

    const auto resources = resourcePool()->getResourcesByIds<QnMediaServerResource>(ids);
    const auto properties = sourceServerField->properties();
    const bool isValidSelection = !resources.empty() || properties.allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return vms::rules::Strings::selectServer();

        if (sourceServerField->acceptAll())
            return Strings::anyServer();

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
            return iconPath(QnResourceIconCache::Servers, rule->enabled() && d->isRuleValid(rule));

        if (resources.size() == 1)
        {
            return iconPath(
                qnResIconCache->key(resources.first()), rule->enabled() && d->isRuleValid(rule));
        }

        return iconPath(QnResourceIconCache::Server, rule->enabled() && d->isRuleValid(rule));
    }

    return {};
}

QVariant RulesTableModel::sourceUserData(const ConstRulePtr& rule, int role) const
{
    const auto eventFilter = rule->eventFilters().first();
    auto sourceUserField = eventFilter->fieldByName<vms::rules::SourceUserField>(
        vms::rules::utils::kUserIdFieldName);
    if (sourceUserField == nullptr)
        return {};

    QnUserResourceList users;
    nx::vms::api::UserGroupDataList groups;
    nx::vms::common::getUsersAndGroups(systemContext(), sourceUserField->ids(), users, groups);

    if (role == ResourceIdsRole)
        return QVariant::fromValue(nx::utils::toQSet(users.ids()));

    return {};
}

QVariant RulesTableModel::actionColumnData(const ConstRulePtr& rule, int role) const
{
    if (role == Qt::DisplayRole)
    {
        return d->engine->actionDescriptor(rule->actionBuilders().first()->actionType())
            ->displayName();
    }

    return {};
}

QVariant RulesTableModel::targetColumnData(const ConstRulePtr& rule, int role) const
{
    const auto actionBuilder = rule->actionBuilders().first();
    const auto actionDescriptor = d->engine->actionDescriptor(actionBuilder->actionType());

    if (role == ResourceIdsRole)
        return QVariant::fromValue(targetIds(rule, actionDescriptor.value()));

    QVariant result;
    if (vms::rules::hasTargetCamera(actionDescriptor.value()))
        result = targetCameraData(rule, role);

    if (result.isNull() && vms::rules::hasTargetLayout(actionDescriptor.value()))
        result = targetLayoutData(rule, role);

    if (result.isNull() && vms::rules::hasTargetUser(actionDescriptor.value()))
        result = targetUserData(rule, role);

    if (result.isNull() && vms::rules::hasTargetServer(actionDescriptor.value()))
        result = targetServerData(rule, role);

    if (result.isNull())
        result = systemData(rule, role);

    return result;
}

QVariant RulesTableModel::targetCameraData(const ConstRulePtr& rule, int role) const
{
    const auto actionBuilder = rule->actionBuilders().first();
    QSet<nx::Uuid> ids;
    QnVirtualCameraResourceList resources;
    bool useSource{false};
    bool acceptAll{false};
    bool hasAdditionalRecipients{false};
    bool allowEmptySelection{false};

    if (const auto targetDeviceField = actionBuilder->fieldByType<vms::rules::TargetDevicesField>())
    {
        const auto targetDeviceFieldProperties = targetDeviceField->properties();

        useSource = targetDeviceField->useSource();
        allowEmptySelection = targetDeviceFieldProperties.allowEmptySelection;
        ids = targetDeviceField->ids();
        resources = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(ids);

        if (targetDeviceFieldProperties.validationPolicy
            == vms::rules::kCameraAudioTransmissionValidationPolicy)
        {
            if (const auto targetUsersField =
                    actionBuilder->fieldByType<vms::rules::TargetUsersField>())
            {
                const auto users =
                    vms::rules::utils::users(targetUsersField->selection(), systemContext());
                hasAdditionalRecipients = !users.isEmpty();
            }
        }
    }
    else if (const auto targetSingleDeviceField =
        actionBuilder->fieldByType<vms::rules::TargetDeviceField>())
    {
        useSource = targetSingleDeviceField->useSource();
        allowEmptySelection = targetSingleDeviceField->properties().allowEmptySelection;
        if (!targetSingleDeviceField->id().isNull())
        {
            ids = {targetSingleDeviceField->id()};
            resources = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(ids);
        }
    }
    else
    {
        return {};
    }

    const bool isValidSelection = useSource
        || !resources.isEmpty()
        || hasAdditionalRecipients
        || allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return vms::rules::Strings::selectCamera(systemContext());

        if (useSource)
        {
            return resources.empty()
                ? Strings::sourceCamera()
                : tr("Source and %n more Cameras", "Numerus: placeholder for more items",
                    static_cast<int>(resources.size()));
        }

        if (resources.isEmpty())
            return {};

        if (resources.size() == 1)
        {
            return QnResourceDisplayInfo(resources.first()).toString(
                appContext()->localSettings()->resourceInfoLevel());
        }

        return QnDeviceDependentStrings::getNumericName(resourcePool(), resources);
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        const auto targetCamerasCount = resources.size() + (useSource ? 1 : 0);

        if (acceptAll || targetCamerasCount > 1)
            return iconPath(QnResourceIconCache::Cameras, rule->enabled() && d->isRuleValid(rule));

        if (targetCamerasCount == 1)
            return iconPath(QnResourceIconCache::Camera, rule->enabled() && d->isRuleValid(rule));

        return {};
    }

    if (role == ResourceIdsRole)
        return QVariant::fromValue(ids);

    return {};
}

QVariant RulesTableModel::targetLayoutData(const ConstRulePtr& rule, int role) const
{
    const auto actionBuilder = rule->actionBuilders().first();
    QSet<Uuid> ids;
    bool allowEmptySelection{false};
    if (auto targetLayoutsField = actionBuilder->fieldByType<vms::rules::TargetLayoutsField>())
    {
        ids = targetLayoutsField->value();
        allowEmptySelection = targetLayoutsField->properties().allowEmptySelection;
    }
    else if (auto targetLayoutField = actionBuilder->fieldByType<vms::rules::TargetLayoutField>())
    {
        ids = {targetLayoutField->value()};
    }
    else
    {
        return {};
    }

    if (role == ResourceIdsRole)
        return QVariant::fromValue(ids);

    const auto layouts = resourcePool()->getResourcesByIds<QnLayoutResource>(ids);
    const bool isValidSelection = !layouts.isEmpty() || allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
        {
            return ids.empty()
                ? tr("Select at least one layout")
                : vms::rules::Strings::layoutsWereRemoved(ids.size());
        }

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
            ? iconPath(QnResourceIconCache::Layouts, rule->enabled() && d->isRuleValid(rule))
            : iconPath(QnResourceIconCache::Layout, rule->enabled() && d->isRuleValid(rule));
    }

    return {};
}

QVariant RulesTableModel::targetUserData(const ConstRulePtr& rule, int role) const
{
    const auto actionBuilder = rule->actionBuilders().first();
    const auto targetUserField = actionBuilder->fieldByType<vms::rules::TargetUsersField>();
    if (!targetUserField)
        return {};

    QnUserResourceList users;
    nx::vms::api::UserGroupDataList groups;
    nx::vms::common::getUsersAndGroups(systemContext(), targetUserField->ids(), users, groups);

    if (role == ResourceIdsRole)
        return QVariant::fromValue(nx::utils::toQSet(users.ids()));

    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

    QStringList additionalRecipients;
    if (const auto actionTextField = actionBuilder->fieldByName<vms::rules::ActionTextField>(
        vms::rules::utils::kEmailsFieldName))
    {
        const auto emails = actionTextField->value().split(';', Qt::SkipEmptyParts);
        for (const auto& email: emails)
        {
            if (const auto trimmed = email.trimmed(); !trimmed.isEmpty())
                additionalRecipients.push_back(trimmed);
        }
    }

    const auto properties = targetUserField->properties();
    const bool isValidSelection = targetUserField->acceptAll()
        || !users.empty()
        || !groups.empty()
        || !additionalRecipients.isEmpty()
        || properties.allowEmptySelection;

    if (role == Qt::DisplayRole)
    {
        if (!isValidSelection)
            return tr("Select at least one User");

        if (targetUserField->acceptAll())
            return tr("All Users");

        return Strings::targetRecipientsString(users, groups, additionalRecipients);
    }

    if (role == Qt::DecorationRole)
    {
        if (!isValidSelection)
            return kAttentionIconPath;

        const auto totalRecipientsCount = users.size() + additionalRecipients.size();

        return (targetUserField->acceptAll() || totalRecipientsCount > 1 || !groups.empty())
            ? iconPath(QnResourceIconCache::Users, rule->enabled() && d->isRuleValid(rule))
            : iconPath(QnResourceIconCache::User, rule->enabled() && d->isRuleValid(rule));
    }

    return {};
}

QVariant RulesTableModel::targetServerData(const ConstRulePtr& rule, int role) const
{
    const auto actionBuilder = rule->actionBuilders().first();
    const auto targetServerField = actionBuilder->fieldByType<vms::rules::TargetServersField>();
    if (!targetServerField)
        return {};

    const auto ids = targetServerField->ids();
    if (role == ResourceIdsRole)
        return QVariant::fromValue(ids);

    const QnMediaServerResourceList targetServers =
        resourcePool()->getResourcesByIds<QnMediaServerResource>(ids);
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
            ? iconPath(QnResourceIconCache::Servers, rule->enabled() && d->isRuleValid(rule))
            : iconPath(QnResourceIconCache::Server, rule->enabled() && d->isRuleValid(rule));
    }

    return {};
}

QVariant RulesTableModel::systemData(const ConstRulePtr& rule, int role) const
{
    if (role == Qt::DisplayRole)
        return tr("Site");

    if (role == Qt::DecorationRole)
    {
        return iconPath(
            QnResourceIconCache::CurrentSystem, rule->enabled() && d->isRuleValid(rule));
    }

    return {};
}

QVariant RulesTableModel::commentColumnData(const ConstRulePtr& rule, int role) const
{
    if (role == Qt::DisplayRole)
        return rule->comment();

    return {};
}

QSet<nx::Uuid> RulesTableModel::resourceIds(int row, int column) const
{
    return data(createIndex(row, column), ResourceIdsRole).value<UuidSet>();
}

QSet<nx::Uuid> RulesTableModel::sourceIds(
    const ConstRulePtr& rule,
    const vms::rules::ItemDescriptor& descriptor) const
{
    const auto eventFilter = rule->eventFilters().first();
    if (!NX_ASSERT(eventFilter->eventType() == descriptor.id))
        return {};

    QSet<nx::Uuid> result;
    if (vms::rules::hasSourceCamera(descriptor))
        result += sourceCameraData(rule, ResourceIdsRole).value<QSet<nx::Uuid>>();

    if (vms::rules::hasSourceServer(descriptor))
        result += sourceServerData(rule, ResourceIdsRole).value<QSet<nx::Uuid>>();

    if (vms::rules::hasSourceUser(descriptor))
        result += sourceUserData(rule, ResourceIdsRole).value<QSet<nx::Uuid>>();

    return result;
}

QSet<nx::Uuid> RulesTableModel::targetIds(
    const ConstRulePtr& rule,
    const vms::rules::ItemDescriptor& descriptor) const
{
    const auto actionBuilder = rule->actionBuilders().first();
    if (!NX_ASSERT(actionBuilder->actionType() == descriptor.id))
        return {};

    QSet<nx::Uuid> result;
    if (vms::rules::hasTargetCamera(descriptor))
        result += targetCameraData(rule, ResourceIdsRole).value<QSet<nx::Uuid>>();

    if (vms::rules::hasTargetLayout(descriptor))
        result += targetLayoutData(rule, ResourceIdsRole).value<QSet<nx::Uuid>>();

    if (vms::rules::hasTargetUser(descriptor))
        result += targetUserData(rule, ResourceIdsRole).value<QSet<nx::Uuid>>();

    if (vms::rules::hasTargetServer(descriptor))
        result += targetServerData(rule, ResourceIdsRole).value<QSet<nx::Uuid>>();

    return result;
}

bool RulesTableModel::hasAnyOf(int row, const QList<nx::Uuid>& resourceIds) const
{
    return
        std::any_of(
            resourceIds.cbegin(),
            resourceIds.cend(),
            [ids = this->resourceIds(row, SourceColumn)](nx::Uuid id) { return ids.contains(id); }) ||
        std::any_of(
            resourceIds.cbegin(),
            resourceIds.cend(),
            [ids = this->resourceIds(row, TargetColumn)](nx::Uuid id) { return ids.contains(id); });
}

bool RulesTableModel::hasResources(int row)
{
    return !resourceIds(row, SourceColumn).empty() || !resourceIds(row, TargetColumn).empty();
}

void RulesTableModel::registerQmlType()
{
    qmlRegisterUncreatableType<rules::RulesTableModel>(
        "nx.vms.client.desktop", 1, 0, "RulesTableModel", "Cannot create RulesTableModel.");
}

} // namespace nx::vms::client::desktop::rules
