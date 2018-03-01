#include "action_buttons_model.h"

#include <nx/client/mobile/ptz/ptz_availability_watcher.h>
#include <nx/client/mobile/software_trigger/software_triggers_watcher.h>
#include <nx/client/core/two_way_audio/two_way_audio_availability_watcher.h>
#include <nx/utils/log/log.h>
#include <nx/vms/event/strings_helper.h>

namespace {

enum Roles
{
    typeRoleId = Qt::UserRole,
    enabledRoleId,
    iconPathRoleId,

    // Software trigger roles
    triggerNameRoleId,
    triggerIdRoleId,
    triggerIsProlongedRoleId,
};

const QHash<int, QByteArray> kRoleNames = {
    {typeRoleId, "type"},
    {enabledRoleId, "enabled"},
    {iconPathRoleId, "iconPath"},

    {triggerNameRoleId, "triggerName"},
    {triggerIdRoleId, "triggerId"},
    {triggerIsProlongedRoleId, "prolongedTrigger"}};

} // namespace

namespace nx {
namespace client {
namespace mobile {

using namespace core;

struct ActionButtonsModel::Button
{
    Button(ActionButtonsModel::ButtonType type, const QString& iconPath, bool enabled);
    virtual ~Button();

    ActionButtonsModel::ButtonType type;
    QString iconPath;
    bool enabled = true;

    static ActionButtonsModel::ButtonPtr ptzButton();
    static ActionButtonsModel::ButtonPtr twoWayAudioButton();
};

ActionButtonsModel::Button::Button(
    ActionButtonsModel::ButtonType type,
    const QString& iconPath,
    bool enabled)
    :
    type(type),
    iconPath(iconPath),
    enabled(enabled)
{
}

ActionButtonsModel::Button::~Button()
{
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::ptzButton()
{
    return ButtonPtr(new Button(
        ActionButtonsModel::PtzButton, lit("images/ptz/ptz.png"), true));
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::twoWayAudioButton()
{
    return ButtonPtr(new Button(
        ActionButtonsModel::TwoWayAudioButton, lit("images/two_way_audio/mic.png"), true));
}

//

struct ActionButtonsModel::SoftwareButton: public ActionButtonsModel::Button
{
    QnUuid id;
    SoftwareTriggerData data;

    static ButtonPtr create(
        const QnUuid& id,
        const SoftwareTriggerData& data,
        const QString& iconPath,
        bool enabled);

    static ButtonPtr fake(const QnUuid& id);

private:
    using base_type = ActionButtonsModel::Button;

    SoftwareButton(const QnUuid& id);
    SoftwareButton(
        const QnUuid& id,
        const SoftwareTriggerData& data,
        const QString& iconPath,
        bool enabled);
};

ActionButtonsModel::ButtonPtr ActionButtonsModel::SoftwareButton::create(
    const QnUuid& id,
    const SoftwareTriggerData& data,
    const QString& iconPath,
    bool enabled)
{
    return ButtonPtr(new SoftwareButton(id, data, iconPath, enabled));
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::SoftwareButton::fake(const QnUuid& id)
{
    return ButtonPtr(new SoftwareButton(id));
}

ActionButtonsModel::SoftwareButton::SoftwareButton(const QnUuid& id):
    base_type(ActionButtonsModel::SoftTriggerButton, QString(), false),
    id(id)
{
}

ActionButtonsModel::SoftwareButton::SoftwareButton(
    const QnUuid& id,
    const SoftwareTriggerData& data,
    const QString& iconPath,
    bool enabled)
    :
    base_type(ActionButtonsModel::SoftTriggerButton, iconPath, enabled),
    id(id),
    data(data)
{
}

//

ActionButtonsModel::ActionButtonsModel(QObject* parent):
    base_type(parent),
    m_ptzAvailabilityWatcher(new PtzAvailabilityWatcher()),
    m_twoWayAudioAvailabilityWatcher(new TwoWayAudioAvailabilityWatcher()),
    m_softwareTriggeresWatcher(new SoftwareTriggersWatcher())
{
    connect(this, &ActionButtonsModel::resourceIdChanged,
        this, &ActionButtonsModel::handleResourceIdChanged);
    connect(m_ptzAvailabilityWatcher, &PtzAvailabilityWatcher::availabilityChanged,
        this, &ActionButtonsModel::updatePtzButtonVisibility);
    connect(m_twoWayAudioAvailabilityWatcher, &TwoWayAudioAvailabilityWatcher::availabilityChanged,
        this, &ActionButtonsModel::updateTwoWayAudioButtonVisibility);

    connect(m_softwareTriggeresWatcher, &SoftwareTriggersWatcher::triggerAdded,
        this, &ActionButtonsModel::addSoftwareTriggerButton);
    connect(m_softwareTriggeresWatcher, &SoftwareTriggersWatcher::triggerRemoved,
        this, &ActionButtonsModel::removeSoftwareTriggerButton);

    const auto emitDataChanged =
        [this](const QnUuid& id, int roleId)
        {
            const auto row = triggerButtonIndexById(id);
            if (row < 0)
            {
                NX_EXPECT(false, "Wrong trigger button id");
                return;
            }

            const auto rowIndex = index(row);
            emit dataChanged(rowIndex, rowIndex, {roleId});
        };

    connect(m_softwareTriggeresWatcher, &SoftwareTriggersWatcher::triggerEnabledChanged,
        this, &ActionButtonsModel::updateTriggerEnabled);
    connect(m_softwareTriggeresWatcher, &SoftwareTriggersWatcher::triggerIconChanged,
        this, &ActionButtonsModel::updateTriggerIcon);
    connect(m_softwareTriggeresWatcher, &SoftwareTriggersWatcher::triggerNameChanged,
        this, &ActionButtonsModel::updateTriggerName);
    connect(m_softwareTriggeresWatcher, &SoftwareTriggersWatcher::triggerIdChanged,
        this, &ActionButtonsModel::updateTriggerId);
    connect(m_softwareTriggeresWatcher, &SoftwareTriggersWatcher::triggerProlongedChanged,
        this, &ActionButtonsModel::updateTriggerProlonged);
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
        case enabledRoleId:
            return button->enabled;
        case iconPathRoleId:
            return button->iconPath;
    }

    const auto softwareButton = button->type == SoftTriggerButton
        ? static_cast<SoftwareButton*>(button.data())
        : nullptr;

    switch(role)
    {
        case triggerNameRoleId:
            if (softwareButton)
                return vms::event::StringsHelper::getSoftwareTriggerName(softwareButton->data.name);
            NX_EXPECT(false, "Wrong software trigger button");
            break;
        case triggerIdRoleId:
            if (softwareButton)
                return softwareButton->data.triggerId;
            NX_EXPECT(false, "Wrong software trigger button");
            break;
        case triggerIsProlongedRoleId:
            if (softwareButton)
                return softwareButton->data.prolonged;
            NX_EXPECT(false, "Wrong software trigger button");
            break;
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
    m_softwareTriggeresWatcher->setResourceId(m_resourceId);
}

int ActionButtonsModel::softTriggerButtonStartIndex() const
{
    const auto it = std::find_if(m_buttons.begin(), m_buttons.end(),
        [](const ButtonPtr& button)
        {
            return button->type == SoftTriggerButton;
        });

    return it == m_buttons.end() ? m_buttons.size() : it - m_buttons.begin();
}

QnUuid ActionButtonsModel::getSoftwareButtonId(const ButtonPtr& button)
{
    const auto softwareButton = dynamic_cast<SoftwareButton*>(button.data());
    return softwareButton ? softwareButton->id : QnUuid();
}

ActionButtonsModel::ButtonList::const_iterator ActionButtonsModel::lowerBoundByTriggerButtonId(
    const QnUuid& id) const
{
    static const auto compareFunction =
        [](const ButtonPtr& left, const ButtonPtr& right)
        {
            const auto leftRuleId = getSoftwareButtonId(left);
            const auto rightRuleId = getSoftwareButtonId(right);
            if (leftRuleId.isNull() || rightRuleId.isNull())
            {
                NX_EXPECT(false, "We expect each button to be software trigger");
                return leftRuleId == rightRuleId ? left < right : leftRuleId < rightRuleId;
            }
            return leftRuleId < rightRuleId;
        };

    const auto startIndex = softTriggerButtonStartIndex();
    return std::lower_bound(
        m_buttons.begin() + startIndex, m_buttons.end(),
        SoftwareButton::fake(id), compareFunction);
}

int ActionButtonsModel::triggerButtonIndexById(const QnUuid& id) const
{
    const auto it = lowerBoundByTriggerButtonId(id);
    return it != m_buttons.end() && getSoftwareButtonId(*it) == id
        ? it - m_buttons.begin()
        : -1;
}

int ActionButtonsModel::triggerButtonInsertionIndexById(const QnUuid& ruleId) const
{
    const auto it = lowerBoundByTriggerButtonId(ruleId);
    return it == m_buttons.end() ? m_buttons.size() : it - m_buttons.begin();
}

void ActionButtonsModel::addSoftwareTriggerButton(
    const QnUuid& id,
    const SoftwareTriggerData& data,
    const QString& iconPath,
    bool enabled)
{
    const auto index = triggerButtonInsertionIndexById(id);
    insertButton(triggerButtonInsertionIndexById(id), SoftwareButton::create(id, data, iconPath, enabled));
}

void ActionButtonsModel::removeSoftwareTriggerButton(const QnUuid& id)
{
    removeButton(triggerButtonIndexById(id));
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

void ActionButtonsModel::updateTriggerEnabled(const QnUuid& id)
{
    const auto row = triggerButtonIndexById(id);
    if (row < 0)
    {
        NX_EXPECT(false, "Software button does not exist");
        return;
    }

    const auto button = static_cast<SoftwareButton*>(m_buttons[row].data());
    const auto enabled = m_softwareTriggeresWatcher->triggerEnabled(id);
    if (button->enabled == enabled)
        return;

    button->enabled = enabled;
    const auto rowIndex = index(row);
    emit dataChanged(rowIndex, rowIndex, {enabledRoleId});
}

void ActionButtonsModel::updateTriggerName(const QnUuid& id)
{
    const auto row = triggerButtonIndexById(id);
    if (row < 0)
    {
        NX_EXPECT(false, "Software button does not exist");
        return;
    }

    const auto button = static_cast<SoftwareButton*>(m_buttons[row].data());
    const auto name = m_softwareTriggeresWatcher->triggerData(id).name;
    if (button->data.name == name)
        return;

    button->data.name = name;
    const auto rowIndex = index(row);
    emit dataChanged(rowIndex, rowIndex, {triggerNameRoleId});
}

void ActionButtonsModel::updateTriggerIcon(const QnUuid& id)
{
    const auto row = triggerButtonIndexById(id);
    if (row < 0)
    {
        NX_EXPECT(false, "Software button does not exist");
        return;
    }

    const auto button = static_cast<SoftwareButton*>(m_buttons[row].data());
    const auto icon = m_softwareTriggeresWatcher->triggerIcon(id);
    if (button->iconPath == icon)
        return;

    button->iconPath = icon;
    const auto rowIndex = index(row);
    emit dataChanged(rowIndex, rowIndex, {iconPathRoleId});
}

void ActionButtonsModel::updateTriggerId(const QnUuid& id)
{
    const auto row = triggerButtonIndexById(id);
    if (row < 0)
    {
        NX_EXPECT(false, "Software button does not exist");
        return;
    }

    const auto button = static_cast<SoftwareButton*>(m_buttons[row].data());
    const auto triggerId = m_softwareTriggeresWatcher->triggerData(id).triggerId;
    if (button->data.triggerId == triggerId)
        return;

    button->data.triggerId = triggerId;
    const auto rowIndex = index(row);
    emit dataChanged(rowIndex, rowIndex, {triggerIdRoleId});
}

void ActionButtonsModel::updateTriggerProlonged(const QnUuid& id)
{
    const auto row = triggerButtonIndexById(id);
    if (row < 0)
    {
        NX_EXPECT(false, "Software button does not exist");
        return;
    }

    const auto button = static_cast<SoftwareButton*>(m_buttons[row].data());
    const auto prolonged = m_softwareTriggeresWatcher->triggerData(id).prolonged;
    if (button->data.prolonged == prolonged)
        return;

    button->data.prolonged = prolonged;
    const auto rowIndex = index(row);
    emit dataChanged(rowIndex, rowIndex, {triggerIsProlongedRoleId});
}

} // namespace mobile
} // namespace client
} // namespace nx
