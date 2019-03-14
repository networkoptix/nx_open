#include "action_buttons_model.h"

#include <QtQml/QtQml>

#include <nx/client/mobile/ptz/ptz_availability_watcher.h>
#include <nx/client/mobile/software_trigger/software_triggers_watcher.h>
#include <nx/client/core/two_way_audio/two_way_audio_availability_watcher.h>
#include <nx/utils/log/log.h>

namespace {

enum Roles
{
    idRoleId = Qt::UserRole,
    typeRoleId,
    iconPathRoleId,
    hintRoleId,
    prolongedActionRoleId,
    disableLongPressRoleId,
    enabledRoleId
};

const QHash<int, QByteArray> kRoleNames = {
    {idRoleId, "id"},
    {typeRoleId, "type"},
    {iconPathRoleId, "iconPath"},
    {hintRoleId, "hint"},
    {prolongedActionRoleId, "prolongedAction"},
    {disableLongPressRoleId, "disableLongPress"},
    {enabledRoleId, "enabled"}};

} // namespace

namespace nx {
namespace client {
namespace mobile {

using namespace nx::vms::client::core;

struct ActionButtonsModel::Button
{
    Button(
        const QnUuid& id,
        ActionButtonsModel::ButtonType type,
        const QString& iconPath,
        const QString& hint,
        bool prolongedAction,
        bool disableLongPress,
        bool enabled);

    virtual ~Button();

    QnUuid id;
    ActionButtonsModel::ButtonType type;
    QString iconPath;
    QString hint;
    bool prolongedAction = false;
    bool disableLongPress = false;
    bool enabled = true;

    static ActionButtonsModel::ButtonPtr createFakeSoftButton(const QnUuid& id);
    static ActionButtonsModel::ButtonPtr createPtzButton();
    static ActionButtonsModel::ButtonPtr createTwoWayAudioButton();
    static ActionButtonsModel::ButtonPtr createSoftwareTriggerButton(
        const QnUuid& id,
        const QString& iconPath,
        const QString& name,
        bool prolonged,
        bool enabled);
};

ActionButtonsModel::Button::Button(
    const QnUuid& id,
    ActionButtonsModel::ButtonType type,
    const QString& iconPath,
    const QString& hint,
    bool prolongedAction,
    bool disableLongPress,
    bool enabled)
    :
    id(id),
    type(type),
    iconPath(iconPath),
    hint(hint),
    prolongedAction(prolongedAction),
    disableLongPress(disableLongPress),
    enabled(enabled)
{
}

ActionButtonsModel::Button::~Button()
{
}

ActionButtonsModel::ActionButtonsModel::ButtonPtr
ActionButtonsModel::Button::createFakeSoftButton(const QnUuid& id)
{
    return ButtonPtr(new Button(id, ActionButtonsModel::SoftTriggerButton,
        QString(), QString(), true, true, true));
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::createPtzButton()
{
    return ButtonPtr(new Button(
        QnUuid(), ActionButtonsModel::PtzButton,
        lit("qrc:///images/ptz/ptz.png"), QString(), false, true, true));
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::createTwoWayAudioButton()
{
    return ButtonPtr(new Button(
        QnUuid(), ActionButtonsModel::TwoWayAudioButton,
        lit("qrc:///images/two_way_audio/mic.png"),
        ActionButtonsModel::twoWayButtonHint(), true, false, true));
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::createSoftwareTriggerButton(
    const QnUuid& id,
    const QString& iconPath,
    const QString& name,
    bool prolonged,
    bool enabled)
{
    return ButtonPtr(new Button(
        id, ActionButtonsModel::SoftTriggerButton,
        iconPath, name, prolonged, false, enabled));
}

//-------------------------------------------------------------------------------------------------

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
            const auto row = rowById(id);
            if (row < 0)
            {
                NX_ASSERT(false, "Wrong trigger button id");
                return;
            }

            const auto rowIndex = index(row);
            emit dataChanged(rowIndex, rowIndex, {roleId});
        };

    connect(m_softwareTriggeresWatcher, &SoftwareTriggersWatcher::triggerFieldsChanged,
        this, &ActionButtonsModel::updateTriggerFields);
}

ActionButtonsModel::~ActionButtonsModel()
{
}

void ActionButtonsModel::registerQmlType()
{
    qmlRegisterType<ActionButtonsModel>("nx.client.mobile", 1, 0, "ActionButtonsModel");
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

    NX_ASSERT(false, "Wrong parent index");
    return 0;
}

QVariant ActionButtonsModel::data(const QModelIndex& index, int role) const
{
    const auto row = index.row();
    if (row >= rowCount())
    {
        NX_ASSERT(false, "Wrong row number");
        return QVariant();
    }

    const auto& button = m_buttons.at(row);
    switch (role)
    {
        case idRoleId:
            return QVariant::fromValue(button->id);
        case typeRoleId:
            return button->type;
        case hintRoleId:
            return button->hint;
        case iconPathRoleId:
            return button->iconPath;
        case prolongedActionRoleId:
            return button->prolongedAction;
        case disableLongPressRoleId:
            return button->disableLongPress;
        case enabledRoleId:
            return button->enabled;
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

ActionButtonsModel::ButtonList::const_iterator ActionButtonsModel::lowerBoundByTriggerButtonId(
    const QnUuid& id) const
{
    static const auto compareFunction =
        [](const ButtonPtr& left, const ButtonPtr& right)
        {
            const auto leftRuleId = left->id;
            const auto rightRuleId = right->id;
            if (leftRuleId.isNull() || rightRuleId.isNull())
            {
                NX_ASSERT(false, "We expect each button to be software trigger");
                return leftRuleId == rightRuleId ? left < right : leftRuleId < rightRuleId;
            }
            return leftRuleId < rightRuleId;
        };

    const auto startIndex = softTriggerButtonStartIndex();
    return std::lower_bound(
        m_buttons.begin() + startIndex, m_buttons.end(),
        Button::createFakeSoftButton(id), compareFunction);
}

int ActionButtonsModel::rowById(const QnUuid& id) const
{
    const auto it = lowerBoundByTriggerButtonId(id);
    return it != m_buttons.end() && (*it)->id == id
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
    const QString& iconPath,
    const QString& name,
    bool prolonged,
    bool enabled)
{
    insertButton(triggerButtonInsertionIndexById(id),
        Button::createSoftwareTriggerButton(id, iconPath, name, prolonged, enabled));
}

void ActionButtonsModel::removeSoftwareTriggerButton(const QnUuid& id)
{
    removeButton(rowById(id));
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
        insertButton(0, Button::createPtzButton());
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

    const bool twoWayAudioButtonVisible = it != itEnd;
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
        insertButton(position, Button::createTwoWayAudioButton());
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
        NX_ASSERT(false, "Wrong button description insertion index");
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
        NX_ASSERT(false, "Wrong button description index");
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_buttons.removeAt(index);
    endRemoveRows();
}

void ActionButtonsModel::updateTriggerFields(
    const QnUuid& id,
    SoftwareTriggersWatcher::TriggerFields fields)
{
    if (fields == SoftwareTriggersWatcher::NoField)
        return;

    const auto row = rowById(id);
    if (row < 0)
    {
        NX_ASSERT(false, "Software button does not exist");
        return;
    }

    const auto button = m_buttons[row].data();

    QVector<int> changedDataIndicies;
    if (fields.testFlag(SoftwareTriggersWatcher::EnabledField)
        && button->enabled != m_softwareTriggeresWatcher->triggerEnabled(id))
    {
        button->enabled = !button->enabled;
        changedDataIndicies << enabledRoleId;
    }

    if (fields.testFlag(SoftwareTriggersWatcher::ProlongedField)
        && button->prolongedAction != m_softwareTriggeresWatcher->prolongedTrigger(id))
    {
        button->prolongedAction = !button->prolongedAction;
        changedDataIndicies << prolongedActionRoleId;
    }

    if (fields.testFlag(SoftwareTriggersWatcher::NameField))
    {
        const auto newName = m_softwareTriggeresWatcher->triggerName(id);
        if (newName != button->hint)
        {
            button->hint = newName;
            changedDataIndicies << hintRoleId;
        }
    }

    if (fields.testFlag(SoftwareTriggersWatcher::IconField))
    {
        const auto newIconPath = m_softwareTriggeresWatcher->triggerIcon(id);
        if (newIconPath != button->iconPath)
        {
            button->iconPath = newIconPath;
            changedDataIndicies << iconPathRoleId;
        }
    }

    const auto rowIndex = index(row);
    emit dataChanged(rowIndex, rowIndex, changedDataIndicies);
}

QString ActionButtonsModel::twoWayButtonHint()
{
    return tr("Press and hold to speak");
}

} // namespace mobile
} // namespace client
} // namespace nx
