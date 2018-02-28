#include "action_buttons_model.h"

#include <nx/client/mobile/ptz/ptz_availability_watcher.h>
#include <nx/client/core/two_way_audio/two_way_audio_availability_watcher.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/utils/log/log.h>

#include <watchers/user_watcher.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/user_roles_manager.h>

namespace {

enum Roles
{
    typeRoleId = Qt::UserRole,
    iconPathRoleId
};

const QHash<int, QByteArray> kRoleNames = {
    {typeRoleId, "type"},
    {iconPathRoleId, "iconPath"}};

QString extractIconPath(const nx::vms::event::RulePtr& rule)
{
    qDebug() << ">>>>>>>>>>>>>>>>>>>>>> " << rule->eventParams().description;
    return lit("images/two_way_audio/mic.png");
}

template<class Container, class Item>
bool contains(const Container& cont, const Item& item)
{
    return std::find(cont.cbegin(), cont.cend(), item) != cont.cend();
}

bool appropriateSoftwareTriggerRule(
    const nx::vms::event::RulePtr& rule,
    const QnUserResourcePtr& currentUser,
    const QnUuid& resourceId)
{
    if (rule->isDisabled() || rule->eventType() != nx::vms::event::softwareTriggerEvent)
        return false;

    if (!rule->eventResources().empty() && !rule->eventResources().contains(resourceId))
        return false;

    if (rule->eventParams().metadata.allUsers)
        return true;

    const auto subjects = rule->eventParams().metadata.instigators;
    if (contains(subjects, currentUser->getId()))
        return true;

    return contains(subjects, QnUserRolesManager::unifiedUserRoleId(currentUser));
}

} // namespace

namespace nx {
namespace client {
namespace mobile {

using namespace core;

struct ActionButtonsModel::Button
{
    Button(ActionButtonsModel::ButtonType type, const QString& iconPath);
    virtual ~Button();

    ActionButtonsModel::ButtonType type;
    QString iconPath;

    static ActionButtonsModel::ButtonPtr ptzButton();
    static ActionButtonsModel::ButtonPtr twoWayAudioButton();
};

ActionButtonsModel::Button::Button(ActionButtonsModel::ButtonType type, const QString& iconPath):
    type(type),
    iconPath(iconPath)
{
}

ActionButtonsModel::Button::~Button()
{
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::ptzButton()
{
    return ButtonPtr(new Button(
        ActionButtonsModel::PtzButton, lit("images/ptz/ptz.png")));
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::twoWayAudioButton()
{
    return ButtonPtr(new Button(
        ActionButtonsModel::TwoWayAudioButton, lit("images/two_way_audio/mic.png")));
}

//

struct ActionButtonsModel::SoftwareButton: public ActionButtonsModel::Button
{
    QnUuid ruleId;

    QString triggerId;
    QString name;
    bool prolonged = false;

    static ButtonPtr create(const nx::vms::event::RulePtr& rule);
    static ButtonPtr fake(const QnUuid& ruleId);

private:
    using base_type = ActionButtonsModel::Button;
    SoftwareButton(const nx::vms::event::RulePtr& rule);
    SoftwareButton(const QnUuid& ruleId);
};

ActionButtonsModel::ButtonPtr ActionButtonsModel::SoftwareButton::create(
    const nx::vms::event::RulePtr& rule)
{
    return ButtonPtr(new SoftwareButton(rule));
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::SoftwareButton::fake(const QnUuid& ruleId)
{
    return ButtonPtr(new SoftwareButton(ruleId));
}

ActionButtonsModel::SoftwareButton::SoftwareButton(const nx::vms::event::RulePtr& rule):
    base_type(ActionButtonsModel::SoftTriggerButton, extractIconPath(rule)),
    ruleId(rule->id()),
    triggerId(rule->eventParams().inputPortId),
    name(rule->eventParams().caption),
    prolonged(rule->isActionProlonged())
{
}

ActionButtonsModel::SoftwareButton::SoftwareButton(const QnUuid& ruleId):
    base_type(ActionButtonsModel::SoftTriggerButton, QString()),
    ruleId(ruleId)
{
}

//

ActionButtonsModel::ActionButtonsModel(QObject* parent):
    base_type(parent),
    m_ptzAvailabilityWatcher(new PtzAvailabilityWatcher()),
    m_twoWayAudioAvailabilityWatcher(new TwoWayAudioAvailabilityWatcher())
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto eventRuleManager = commonModule->eventRuleManager();

    connect(eventRuleManager, &vms::event::RuleManager::resetRules,
        this, &ActionButtonsModel::updateSoftTriggerButtons);
    // TODO: add processing //rule added // rule removed etc

    connect(m_ptzAvailabilityWatcher, &PtzAvailabilityWatcher::availabilityChanged,
        this, &ActionButtonsModel::updatePtzButtonVisibility);
    connect(m_twoWayAudioAvailabilityWatcher, &TwoWayAudioAvailabilityWatcher::availabilityChanged,
        this, &ActionButtonsModel::updateTwoWayAudioButtonVisibility);

    connect(this, &ActionButtonsModel::resourceIdChanged,
        this, &ActionButtonsModel::handleResourceIdChanged);

    updatePtzButtonVisibility();
    updateTwoWayAudioButtonVisibility();
}

ActionButtonsModel::~ActionButtonsModel()
{
}

void ActionButtonsModel::setResourceId(const QString& resourceId)
{
    const auto uuid = QnUuid::fromStringSafe(resourceId);
    if (uuid == m_resourceId)
        return;

    m_resourceId = uuid;
    emit resourceIdChanged();
}

QString ActionButtonsModel::resourceId() const
{
    return m_resourceId.toString();
}

int ActionButtonsModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_buttons.size();

    NX_EXPECT(false, "Wrong parent index");
    return 0;
}

QVariant ActionButtonsModel::data(const QModelIndex& index, int role) const
{
    const auto row = index.row();
    if (row >= rowCount())
    {
        NX_EXPECT(false, "Wrong row number");
        return QVariant();
    }

    const auto& button = m_buttons.at(row);
    switch (role)
    {
        case typeRoleId:
            return button->type;
        case iconPathRoleId:
            return button->iconPath;
        default:
            return QVariant();
    }
    return QVariant();
}

QHash<int, QByteArray> ActionButtonsModel::roleNames() const
{
    return kRoleNames;
}

void ActionButtonsModel::handleResourceIdChanged()
{
    m_ptzAvailabilityWatcher->setResourceId(m_resourceId);
    m_twoWayAudioAvailabilityWatcher->setResourceId(m_resourceId);

    removeSoftTriggerButtons();
    updateSoftTriggerButtons();
}

int ActionButtonsModel::softTriggerButtonStartIndex() const
{
    const auto it = std::find_if(m_buttons.begin(), m_buttons.end(),
        [](const ButtonPtr& button)
        {
            return button->type == SoftTriggerButton;
        });

    return it == m_buttons.end() ? 0 : it - m_buttons.begin();
}

void ActionButtonsModel::removeSoftTriggerButtons()
{
    const auto startIndex = softTriggerButtonStartIndex();
    const auto count = m_buttons.count();
    if (startIndex == count)
        return;

    const auto finishIndex = count - 1;
    beginRemoveRows(QModelIndex(), startIndex, finishIndex);
    m_buttons.erase(m_buttons.begin() + startIndex, m_buttons.end());
    endRemoveRows();
}

void ActionButtonsModel::updateSoftTriggerButtons()
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto accessManager = commonModule->resourceAccessManager();
    const auto userWatcher = commonModule->instance<QnUserWatcher>();
    const auto currentUser = userWatcher->user();
    if (!accessManager->hasGlobalPermission(currentUser, Qn::GlobalUserInputPermission))
        return;

    using ButtonsMap = QMap<QnUuid, ButtonPtr>;
    using ButtonRuleIdsSet = QSet<QnUuid>;

    const auto softwareButtonsStartIndex = softTriggerButtonStartIndex();
    const auto oldButtons =
        [this, softwareButtonsStartIndex]() -> ButtonsMap
        {
            ButtonsMap result;
            std::for_each(m_buttons.begin() + softwareButtonsStartIndex, m_buttons.end(),
                [&result](const ButtonPtr& button)
                {
                    if (const auto softwareButton = dynamic_cast<SoftwareButton*>(button.data()))
                        result.insert(softwareButton->ruleId, button);
                    else
                        NX_EXPECT(false, "button is not for software trigger");
                });
            return result;
        }();

    ButtonsMap newButtons;
    for(const auto& rule: commonModule->eventRuleManager()->rules())
    {
        if (appropriateSoftwareTriggerRule(rule, currentUser, m_resourceId))
            newButtons.insert(rule->id(), SoftwareButton::create(rule));
    }

    const auto oldButtonIds = oldButtons.keys().toSet();
    const auto newButtonIds = newButtons.keys().toSet();

    auto removedButtonIds = ButtonRuleIdsSet(oldButtonIds).subtract(newButtonIds);
    auto addedButtonIds = ButtonRuleIdsSet(newButtonIds).subtract(oldButtonIds);

    for(const auto& ruleId: removedButtonIds)
        removeButton(buttonIndexByRuleId(ruleId));

    for (const auto& ruleId: addedButtonIds)
        insertButton(insertionIndexByRuleId(ruleId), newButtons[ruleId]);
}

QnUuid ActionButtonsModel::getRuleId(const ButtonPtr& button)
{
    const auto softwareButton = dynamic_cast<SoftwareButton*>(button.data());
    return softwareButton ? softwareButton->ruleId : QnUuid();
}

ActionButtonsModel::ButtonList::const_iterator ActionButtonsModel::lowerBoundByRuleId(
    const QnUuid& ruleId) const
{
    static const auto compareFunction =
        [](const ButtonPtr& left, const ButtonPtr& right)
        {
            const auto leftRuleId = getRuleId(left);
            const auto rightRuleId = getRuleId(right);
            if (leftRuleId.isNull() || rightRuleId.isNull())
            {
                NX_EXPECT(false, "We expect each button to be software trigger");
                return leftRuleId == rightRuleId ? left < right : leftRuleId < rightRuleId;
            }
            return leftRuleId < rightRuleId;
        };

    return std::lower_bound(
        m_buttons.begin() + softTriggerButtonStartIndex(), m_buttons.end(),
        SoftwareButton::fake(ruleId), compareFunction);
}

int ActionButtonsModel::buttonIndexByRuleId(const QnUuid& ruleId) const
{
    const auto it = lowerBoundByRuleId(ruleId);
    return it != m_buttons.end() && getRuleId(*it) == ruleId
        ? it - m_buttons.begin()
        : -1;
}

int ActionButtonsModel::insertionIndexByRuleId(const QnUuid& ruleId) const
{
    const auto it = lowerBoundByRuleId(ruleId);
    return it == m_buttons.end() ? m_buttons.size() : it - m_buttons.begin();
}

void ActionButtonsModel::updatePtzButtonVisibility()
{
    const bool visible = ptzButtonVisible();
    const bool ptzAvailable = m_ptzAvailabilityWatcher->available();
    if (ptzAvailable == visible)
        return;

    if (visible)
        removeButton(0);
    else
        insertButton(0, Button::ptzButton());
}

void ActionButtonsModel::updateTwoWayAudioButtonVisibility()
{
    static constexpr int kMaxButtonPosition = 2;

    const auto itEnd = m_buttons.size() > kMaxButtonPosition
        ? m_buttons.begin() + kMaxButtonPosition
        : m_buttons.end();

    const auto it = std::find_if(m_buttons.begin(), itEnd,
        [](const ButtonPtr& button)
        {
            return button->type == ActionButtonsModel::TwoWayAudioButton;
        });

    const bool twoWayAudioButtonVisible = it != m_buttons.end();
    const bool twoWayAudioAvailable = m_twoWayAudioAvailabilityWatcher->available();
    if (twoWayAudioButtonVisible == twoWayAudioAvailable)
        return;

    if (twoWayAudioButtonVisible)
    {
        const auto index = it - m_buttons.begin();
        removeButton(index);
    }
    else
    {
        const auto position = ptzButtonVisible() ? kMaxButtonPosition - 1 : 0;
        insertButton(position, Button::twoWayAudioButton());
    }
}

bool ActionButtonsModel::ptzButtonVisible() const
{
    return !m_buttons.isEmpty() && m_buttons.front()->type == PtzButton;
}

void ActionButtonsModel::insertButton(int index, const ButtonPtr& button)
{
    if (index > m_buttons.size())
    {
        NX_EXPECT(false, "Wrong button description insertion index");
        return;
    }

    beginInsertRows(QModelIndex(), index, index);
    m_buttons.insert(index, button);
    endInsertRows();
}

void ActionButtonsModel::removeButton(int index)
{
    if (index >= m_buttons.size() || index < 0)
    {
        NX_EXPECT(false, "Wrong button description index");
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_buttons.removeAt(index);
    endRemoveRows();
}

} // namespace mobile
} // namespace client
} // namespace nx
