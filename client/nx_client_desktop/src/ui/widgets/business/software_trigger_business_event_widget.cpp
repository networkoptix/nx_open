#include "software_trigger_business_event_widget.h"
#include "ui_software_trigger_business_event_widget.h"

#include <QtCore/QtMath>
#include <QtCore/QScopedValueRollback>

#include <business/business_strings_helper.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>
#include <ui/common/aligner.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/client/desktop/ui/event_rules/subject_selection_dialog.h>

using namespace nx::client::desktop::ui;

namespace {

static constexpr int kDropdownIconSize = 40;

} // namespace

QnSoftwareTriggerBusinessEventWidget::QnSoftwareTriggerBusinessEventWidget(QWidget* parent) :
    base_type(parent),
    ui(new Ui::SoftwareTriggerBusinessEventWidget),
    m_helper(new QnBusinessStringsHelper(commonModule()))
{
    ui->setupUi(this);

    ui->usersButton->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->triggerIdLineEdit->setPlaceholderText(QnBusinessStringsHelper::defaultSoftwareTriggerName());

    connect(ui->triggerIdLineEdit, &QLineEdit::textChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
    connect(ui->iconComboBox, QnComboboxCurrentIndexChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
    connect(ui->usersButton, &QPushButton::clicked, this,
        &QnSoftwareTriggerBusinessEventWidget::at_usersButton_clicked);

    const auto nextEvenValue =
        [](int value) { return value + (value & 1); };

    const auto columnCount = nextEvenValue(qCeil(qSqrt(
        QnSoftwareTriggerPixmaps::pixmapNames().size())));

    ui->iconComboBox->setColumnCount(columnCount);

    ui->iconComboBox->setItemSize({ kDropdownIconSize, kDropdownIconSize });

    ui->iconComboBox->setPixmaps(
        QnSoftwareTriggerPixmaps::pixmapsPath(),
        QnSoftwareTriggerPixmaps::pixmapNames());

    auto aligner = new QnAligner(this);
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

void QnSoftwareTriggerBusinessEventWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(QnBusiness::EventParamsField))
    {
        const auto params = model()->eventParams();
        ui->triggerIdLineEdit->setText(params.caption);
        ui->iconComboBox->setCurrentIcon(
            QnSoftwareTriggerPixmaps::effectivePixmapName(params.description));

        updateUsersButton();
    }
}

void QnSoftwareTriggerBusinessEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    auto eventParams = model()->eventParams();

    eventParams.caption = ui->triggerIdLineEdit->text().trimmed();
    eventParams.description = ui->iconComboBox->currentIcon();

    model()->setEventParams(eventParams);
}

void QnSoftwareTriggerBusinessEventWidget::at_usersButton_clicked()
{
    SubjectSelectionDialog dialog(this);
    auto params = model()->eventParams();

    QSet<QnUuid> selected;
    for (const auto& id: params.metadata.instigators)
        selected.insert(id);

    dialog.setCheckedSubjects(selected);
    dialog.setAllUsers(params.metadata.allUsers);

    const auto updateAlert =
        [&dialog]()
        {
            dialog.showAlert(!dialog.allUsers() && dialog.checkedSubjects().empty()
                ? QnBusinessStringsHelper::needToSelectUserText()
                : QString());
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
    const auto params = model()->eventParams();

    if (params.metadata.allUsers)
    {
        ui->usersButton->setText(QnBusinessStringsHelper::allUsersText());
        ui->usersButton->setIcon(qnResIconCache->icon(QnResourceIconCache::Users));
    }
    else
    {
        QnUserResourceList users;
        QList<QnUuid> roles;
        userRolesManager()->usersAndRoles(params.metadata.instigators, users, roles);

        if (users.isEmpty() && roles.isEmpty())
        {
            ui->usersButton->setText(QnBusinessStringsHelper::needToSelectUserText());
            ui->usersButton->setIcon(qnSkin->icon(lit("tree/user_alert.png")));
        }
        else
        {
            const bool multiple = params.metadata.allUsers || users.size() > 1 || !roles.empty();

            ui->usersButton->setText(m_helper->actionSubjects(users, roles));
            ui->usersButton->setIcon(qnResIconCache->icon(multiple
                ? QnResourceIconCache::Users
                : QnResourceIconCache::User));
        }
    }
}
