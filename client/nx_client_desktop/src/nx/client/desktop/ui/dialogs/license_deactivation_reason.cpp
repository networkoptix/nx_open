#include "license_deactivation_reason.h"

#include "ui_license_deactivation_reason_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>

#include <ui/widgets/common/input_field.h>

#include <nx/client/desktop/ui/common/text_edit.h>

namespace {

template<typename FieldType>
FieldType* createField(const Qn::TextValidateFunction& validator)
{
    auto result = new FieldType();
    result->setValidator(validator);
    return result;
}

} // unnamed namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

LicenseDeactivationReason::LicenseDeactivationReason(
    const QnUserResourcePtr& currentUser,
    QWidget* parent)
    :
    base_type(QnMessageBoxIcon::Information,
        tr("Please fill up information about yourself and reason for license deactivation"),
        QString(), QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, parent)
{
    const auto nextButton = addButton(tr("Next"),
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);

    setDefaultButton(nextButton);
    setEscapeButton(QDialogButtonBox::Cancel);

    addCustomWidget(createWidget(), Layout::Main, 0, Qt::Alignment(), true);
}

LicenseDeactivationReason::~LicenseDeactivationReason()
{
}

QWidget* LicenseDeactivationReason::createWidget()
{
    auto widget = new QWidget();
    auto layout = new QVBoxLayout(widget);

    layout->addWidget(new QLabel(tr("Name")));
    layout->addWidget(createField<QnInputField>(
        Qn::defaultNonEmptyValidator(tr("Name is necessary"))));

    layout->addWidget(new QLabel(tr("Email")));
    layout->addWidget(createField<QnInputField>(
        Qn::defaultEmailValidator(false)));

    layout->addWidget(new QLabel(tr("Reason for deactivation")));
    layout->addWidget(createField<nx::client::desktop::ui::TextEditField>(
        Qn::defaultNonEmptyValidator(tr("Reason is necessary"))));

    return widget;
}

QString LicenseDeactivationReason::name() const
{
    return QString();
}

QString LicenseDeactivationReason::email() const
{
    return QString();
}

QString LicenseDeactivationReason::reason() const
{
    return QString();
}

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

