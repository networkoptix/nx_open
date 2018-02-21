#include "action_buttons_model.h"

#include <nx/client/mobile/ptz/ptz_availability_watcher.h>
#include <nx/client/core/two_way_audio/two_way_audio_availability_watcher.h>
#include <nx/utils/log/log.h>

namespace {

enum Roles
{
    typeRoleId = Qt::UserRole,
    iconPathRoleId
};

const QHash<int, QByteArray> kRoleNames = {
    {typeRoleId, "type"},
    {iconPathRoleId, "iconPath"}};

} // namespace

namespace nx {
namespace client {
namespace mobile {

using namespace core;

struct ActionButtonsModel::Button
{
    ActionButtonsModel::ButtonType type;
    QString iconPath;

    static ActionButtonsModel::ButtonPtr ptzButton();
    static ActionButtonsModel::ButtonPtr twoWayAudioButton();
};

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::ptzButton()
{
    return ButtonPtr(new Button(
        {ActionButtonsModel::PtzButton, lit("images/ptz/ptz.png")}));
}

ActionButtonsModel::ButtonPtr ActionButtonsModel::Button::twoWayAudioButton()
{
    return ButtonPtr(new Button(
        {ActionButtonsModel::TwoWayAudioButton, lit("images/plus.png")}));
}

//

ActionButtonsModel::ActionButtonsModel(QObject* parent):
    base_type(parent),
    m_ptzAvailabilityWatcher(new PtzAvailabilityWatcher()),
    m_twoWayAudioAvailabilityWatcher(new TwoWayAudioAvailabilityWatcher())
{
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
}

void ActionButtonsModel::updatePtzButtonVisibility()
{
    const bool visible = ptzButtonVisible();
    const bool ptzAvailable = true || m_ptzAvailabilityWatcher->available();
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
    const bool twoWayAudioAvailable = true || m_twoWayAudioAvailabilityWatcher->available();
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
    if (index >= m_buttons.size())
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
