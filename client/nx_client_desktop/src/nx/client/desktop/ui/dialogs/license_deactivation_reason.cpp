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

    addCustomWidget(createWidget(nextButton), Layout::Main, 0, Qt::Alignment(), true);
}

LicenseDeactivationReason::~LicenseDeactivationReason()
{
}

QWidget* LicenseDeactivationReason::createWidget(QPushButton* nextButton)
{
    auto widget = new QWidget();
    widget->setFocusPolicy(Qt::TabFocus);

    auto layout = new QVBoxLayout(widget);

    const auto addLabel = [layout](const QString& name) { layout->addWidget(new QLabel(name)); };

    using namespace nx::client::desktop::ui::detail;
    const auto addWidget =
        [this, widget, layout, nextButton](BaseInputField* field, bool forceFocus = false)
        {
            const auto handleChanges =
                [this, nextButton](BaseInputField* field)
                {
                    if (field->isValid())
                        m_invalidFields.remove(field);
                    else
                        m_invalidFields.insert(field);

                    nextButton->setEnabled(m_invalidFields.isEmpty());
                };

            handleChanges(field);
            connect(field, &BaseInputField::isValidChanged, this,
                [field, handleChanges]() { handleChanges(field); });

            layout->addWidget(field);

            if (forceFocus)
                widget->setFocusProxy(field);
        };

    const auto nameField = createField<QnInputField>(
        Qn::defaultNonEmptyValidator(tr("Name is necessary")));
    const auto emailField = createField<QnInputField>(
        Qn::defaultEmailValidator(false));
    const auto reasonField = createField<nx::client::desktop::ui::TextEditField>(
        Qn::defaultNonEmptyValidator(tr("Reason is necessary")));

    connect(nextButton, &QPushButton::clicked, this,
        [this, nameField, emailField, reasonField]()
        {
            m_name = nameField->text();
            m_email = emailField->text();
            m_reason = reasonField->text().split(lit("\n"));
        });

    addLabel(tr("Name"));
    addWidget(nameField, true);
    addLabel(tr("Email"));
    addWidget(emailField);
    addLabel(tr("Reason for deactivation"));
    addWidget(reasonField);
    return widget;
}

QString LicenseDeactivationReason::name() const
{
    return m_name;
}

QString LicenseDeactivationReason::email() const
{
    return m_email;
}

QStringList LicenseDeactivationReason::reason() const
{
    return m_reason;
}

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

