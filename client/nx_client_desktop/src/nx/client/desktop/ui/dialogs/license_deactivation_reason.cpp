#include "license_deactivation_reason.h"

#include "ui_license_deactivation_reason_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>

#include <ui/widgets/common/input_field.h>

#include <nx/client/desktop/ui/common/text_edit_field.h>

namespace {

template<typename FieldType>
FieldType* createField(const QString& text, const Qn::TextValidateFunction& validator)
{
    auto result = new FieldType();
    result->setValidator(validator);
    result->setText(text);
    return result;
}

} // unnamed namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

LicenseDeactivationReason::LicenseDeactivationReason(
    const license::RequestInfo& info,
    QWidget* parent)
    :
    base_type(QnMessageBoxIcon::Information,
        tr("Please fill up information about yourself and reason for license deactivation"),
        QString(), QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, parent),
    m_info(info)
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

    const auto nameField = createField<QnInputField>(m_info.name,
        Qn::defaultNonEmptyValidator(tr("Name is necessary")));
    const auto emailField = createField<QnInputField>(m_info.email,
        Qn::defaultEmailValidator(false));
    const auto reasonField = createField<nx::client::desktop::ui::TextEditField>(
        m_info.reason.join(lit("n")),
        Qn::defaultNonEmptyValidator(tr("Reason is necessary")));

    connect(nextButton, &QPushButton::clicked, this,
        [this, nameField, emailField, reasonField]()
        {
            m_info = nx::client::desktop::license::RequestInfo({
                nameField->text(), emailField->text(),
                reasonField->text().split(lit("\n"))});
        });

    addLabel(tr("Name"));
    addWidget(nameField, true);
    addLabel(tr("Email"));
    addWidget(emailField);
    addLabel(tr("Reason for deactivation"));
    addWidget(reasonField);
    return widget;
}

license::RequestInfo LicenseDeactivationReason::info()
{
    return m_info;
}

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

