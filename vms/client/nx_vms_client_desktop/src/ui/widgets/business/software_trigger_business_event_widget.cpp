// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_business_event_widget.h"
#include "ui_software_trigger_business_event_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QtMath>

#include <business/business_resource_validation.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/workaround/widgets_signals_workaround.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

static constexpr int kDropdownIconSize = 40;

} // namespace

QnSoftwareTriggerBusinessEventWidget::QnSoftwareTriggerBusinessEventWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::SoftwareTriggerBusinessEventWidget),
    m_helper(new vms::event::StringsHelper(systemContext)),
    m_validationPolicy(new QnRequiredPermissionSubjectPolicy(
        Qn::SoftTriggerPermission,
        tr("User Input")))
{
    ui->setupUi(this);

    ui->usersButton->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->triggerIdLineEdit->setPlaceholderText(
        nx::vms::event::StringsHelper::defaultSoftwareTriggerName());

    connect(ui->triggerIdLineEdit, &QLineEdit::textChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
    connect(ui->iconComboBox, QnComboboxCurrentIndexChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
    connect(ui->usersButton, &QPushButton::clicked, this,
        &QnSoftwareTriggerBusinessEventWidget::at_usersButton_clicked);

    const auto nextEvenValue =
        [](int value) { return value + (value & 1); };

    const auto columnCount = nextEvenValue(qCeil(qSqrt(
        SoftwareTriggerPixmaps::pixmapNames().size())));

    ui->iconComboBox->setColumnCount(columnCount);

    ui->iconComboBox->setItemSize({ kDropdownIconSize, kDropdownIconSize });

    ui->iconComboBox->setPixmaps(
        SoftwareTriggerPixmaps::pixmapsPath(),
        SoftwareTriggerPixmaps::pixmapNames());

    auto aligner = new Aligner(this);
    aligner->addWidget(ui->nameLabel);
    aligner->addWidget(ui->iconLabel);
    aligner->addWidget(ui->usersLabel);
}

QnSoftwareTriggerBusinessEventWidget::~QnSoftwareTriggerBusinessEventWidget()
{
}

void QnSoftwareTriggerBusinessEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->usersButton);
    setTabOrder(ui->iconComboBox, after);
}

void QnSoftwareTriggerBusinessEventWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(Field::eventResources))
    {
        auto cameras =
            resourcePool()->getResourcesByIds<QnVirtualCameraResource>(model()->eventResources());
        m_validationPolicy->setCameras(cameras);
    }

    if (fields.testFlag(Field::eventParams))
    {
        const auto params = model()->eventParams();
        ui->triggerIdLineEdit->setText(params.getTriggerName());
        ui->iconComboBox->setCurrentIcon(
            SoftwareTriggerPixmaps::effectivePixmapName(params.getTriggerIcon()));

        updateUsersButton();
    }
}

void QnSoftwareTriggerBusinessEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    auto eventParams = model()->eventParams();

    eventParams.setTriggerName(ui->triggerIdLineEdit->text().trimmed());
    eventParams.setTriggerIcon(ui->iconComboBox->currentIcon());

    model()->setEventParams(eventParams);
}

void QnSoftwareTriggerBusinessEventWidget::at_usersButton_clicked()
{
    SubjectSelectionDialog dialog(this);
    auto params = model()->eventParams();

    QSet<QnUuid> selected;
    for (const auto& id: params.metadata.instigators)
        selected.insert(id);

    const auto roleValidator =
        [this](const QnUuid& roleId) { return m_validationPolicy->roleValidity(roleId); };

    const auto userValidator =
        [this](const QnUserResourcePtr& user) { return m_validationPolicy->userValidity(user); };

    dialog.setCheckedSubjects(selected);
    dialog.setAllUsers(params.metadata.allUsers);
    dialog.setRoleValidator(roleValidator);
    dialog.setUserValidator(userValidator);

    const auto updateAlert =
        [this, &dialog]()
        {
            dialog.showAlert(m_validationPolicy->calculateAlert(
                dialog.allUsers(), dialog.checkedSubjects()));
        };

    connect(&dialog, &SubjectSelectionDialog::changed, this, updateAlert);
    updateAlert();

    if (dialog.exec() != QDialog::Accepted)
        return;

    params.metadata.allUsers = dialog.allUsers();
    if (!params.metadata.allUsers)
    {
        selected = dialog.checkedSubjects();
        params.metadata.instigators.clear();
        params.metadata.instigators.reserve(selected.size());
        params.metadata.instigators.insert(params.metadata.instigators.end(),
            selected.constBegin(), selected.constEnd());
    }

    model()->setEventParams(params);
    updateUsersButton();
}

void QnSoftwareTriggerBusinessEventWidget::updateUsersButton()
{
    static const QColor mainColor = "#A5B7C0";
    static const nx::vms::client::core::SvgIconColorer::IconSubstitutions colorSubs = {
        {QnIcon::Normal, {{mainColor, "light10"}}}};

    const auto params = model()->eventParams();

    const auto icon =
        [](const QString& path) -> QIcon
        {
            return qnSkin->icon(path, QString(), {}, colorSubs);
        };

    if (params.metadata.allUsers)
    {
        const auto users = resourcePool()->getResources<QnUserResource>()
            .filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

        const bool allValid =
            std::all_of(users.begin(), users.end(),
                [this](const QnUserResourcePtr& user)
                {
                    return m_validationPolicy->userValidity(user);
                });

        ui->usersButton->setText(vms::event::StringsHelper::allUsersText());
        ui->usersButton->setIcon(icon(allValid
            ? lit("tree/users.svg")
            : lit("tree/users_alert.svg")));
    }
    else
    {
        QnUserResourceList users;
        UserGroupDataList groups;
        nx::vms::common::getUsersAndGroups(systemContext(),
            params.metadata.instigators, users, groups);

        users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

        if (users.empty() && groups.empty())
        {
            ui->usersButton->setText(vms::event::StringsHelper::needToSelectUserText());
            ui->usersButton->setIcon(qnSkin->icon(lit("tree/user_alert.svg"), colorSubs));
        }
        else
        {
            const bool allValid =
                std::all_of(users.begin(), users.end(),
                    [this](const QnUserResourcePtr& user)
                    {
                        return m_validationPolicy->userValidity(user);
                    })
                && std::all_of(groups.begin(), groups.end(),
                    [this](const UserGroupData& group)
                    {
                        return m_validationPolicy->roleValidity(group.id) == QValidator::Acceptable;
                    });

            // TODO: #vkutin #3.2 Color the button red if selection is completely invalid.

            const bool multiple = users.size() > 1 || !groups.empty();
            ui->usersButton->setText(m_helper->actionSubjects(users, groups));
            ui->usersButton->setIcon(icon(multiple
                ? (allValid ? lit("tree/users.svg") : lit("tree/users_alert.svg"))
                : (allValid ? lit("tree/user.svg") : lit("tree/user_alert.svg"))));
        }
    }
}
