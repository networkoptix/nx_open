// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_buttons_model.h"

#include <QtQml/QtQml>

#include <nx/utils/algorithm/comparator.h>
#include <nx/vms/client/core/camera/buttons/camera_button_data.h>
#include <nx/vms/client/mobile/camera/buttons/camera_button_controller.h>

namespace nx::vms::client::mobile {

namespace {

using CameraButtonData = core::CameraButtonData;
using AbstractCameraButtonController = core::AbstractCameraButtonController;

enum Roles
{
    id = Qt::UserRole,
    name,
    hint,
    iconPath,
    type,
    enabled,
    group
};

QString getIconPath(int group, const QString& iconName)
{
    return group == static_cast<int>(CameraButtonController::ButtonGroup::ptz)
        ? iconName
        : QStringLiteral("qrc:///skin/soft_triggers/user_selectable/%1.svg").arg(iconName);
}

} // namespace

struct CameraButtonsModel::Private: public QObject
{
    CameraButtonsModel * const q;

    AbstractCameraButtonController* controller;
    UuidList buttons;

    Private(CameraButtonsModel* q);

    void handleButtonAdded(const CameraButtonData& button);
    void handleButtonChanged(const CameraButtonData& button, CameraButtonData::Fields fields);
    void handleButtonRemoved(const nx::Uuid& id);

    void handleControllerRemoved();
    void handleControllerChanged();
};

CameraButtonsModel::Private::Private(CameraButtonsModel* q):
    q(q)
{
}

void CameraButtonsModel::Private::handleButtonAdded(const CameraButtonData& button)
{
    static const auto lessPredicate = nx::utils::algorithm::Comparator(
        /*ascending*/ true, &CameraButtonData::group, &CameraButtonData::name, &CameraButtonData::id);
    const auto itInsert = std::lower_bound(buttons.begin(), buttons.end(), button.id,
        [this, pred = lessPredicate](const nx::Uuid& leftId, const nx::Uuid& rightId)
        {
            return pred(*controller->buttonData(leftId), *controller->buttonData(rightId));
        });

    const int insertionIndex = std::distance(buttons.begin(), itInsert);
    CameraButtonsModel::ScopedInsertRows insertRows(q, insertionIndex, insertionIndex);
    buttons.insert(itInsert, button.id);
}

void CameraButtonsModel::Private::handleButtonChanged(
    const CameraButtonData& button,
    CameraButtonData::Fields /*fields*/)
{
    handleButtonRemoved(button.id);
    handleButtonAdded(button);
}

void CameraButtonsModel::Private::handleButtonRemoved(const nx::Uuid& id)
{
    const auto it = std::find(buttons.begin(), buttons.end(), id);
    if (it == buttons.end())
        return;

    controller->cancelAction(id);

    const int index = std::distance(buttons.begin(), it);
    CameraButtonsModel::ScopedRemoveRows removeRows(q, index, index);
    buttons.erase(it);
}

void CameraButtonsModel::Private::handleControllerRemoved()
{
    CameraButtonsModel::ScopedReset reset(q);
    buttons.clear();
}

void CameraButtonsModel::Private::handleControllerChanged()
{
    if (!controller)
        return;

    connect(controller, &AbstractCameraButtonController::buttonAdded,
        this, &Private::handleButtonAdded);
    connect(controller, &AbstractCameraButtonController::buttonChanged,
        this, &Private::handleButtonChanged);
    connect(controller, &AbstractCameraButtonController::buttonRemoved,
        this, &Private::handleButtonRemoved);

    for (const auto& button: controller->buttonsData())
        handleButtonAdded(button);
}

//-------------------------------------------------------------------------------------------------

void CameraButtonsModel::registerQmlType()
{
    qmlRegisterType<CameraButtonsModel>("nx.vms.client.core", 1, 0, "CameraButtonsModel");
}

CameraButtonsModel::CameraButtonsModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

CameraButtonsModel::~CameraButtonsModel()
{
}

AbstractCameraButtonController* CameraButtonsModel::controller() const
{
    return d->controller;
}

void CameraButtonsModel::setController(AbstractCameraButtonController* value)
{
    if (value == d->controller)
        return;

    d->handleControllerRemoved();
    d->controller = value;
    d->handleControllerChanged();
    emit controllerChanged();
}

int CameraButtonsModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return d->buttons.size();

    NX_ASSERT(false, "Wrong parent index");
    return 0;
}

QVariant CameraButtonsModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row > rowCount())
    {
        NX_ASSERT(false, "Wrong index!");
        return {};
    }

    const auto& id = d->buttons.at(row);
    const auto& button = d->controller->buttonData(id);

    switch (role)
    {
        case Roles::id:
            return QVariant::fromValue(button->id);
        case Roles::name:
            return button->name;
        case Roles::hint:
            return button->hint;
        case Roles::iconPath:
            return getIconPath(button->group, button->iconName);
        case Roles::type:
            return static_cast<int>(button->type);
        case Roles::enabled:
            return button->enabled;
        case Roles::group:
            return button->group;
        default:
            return {};
    }
}

QHash<int, QByteArray> CameraButtonsModel::roleNames() const
{
    static const QHash<int, QByteArray> kRoleNames = {
        {Roles::id, "id"},
        {Roles::name, "name"},
        {Roles::hint, "hint"},
        {Roles::iconPath, "iconPath"},
        {Roles::type, "type"},
        {Roles::enabled, "enabled"},
        {Roles::group, "group"}};
    return kRoleNames;
}

int CameraButtonsModel::rowById(const nx::Uuid& buttonId) const
{
    const auto it = std::find(d->buttons.begin(), d->buttons.end(), buttonId);
    return it == d->buttons.end()
        ? 0
        : std::distance(d->buttons.begin(), it);
}

} // namespace nx::vms::client::mobile
