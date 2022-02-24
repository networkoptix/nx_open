// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edit_property_dialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>

namespace nx::vms::client::desktop {

struct EditPropertyDialog::Private
{
    EditPropertyDialog* const q;

    QLineEdit* const nameEdit = new QLineEdit(q);
    QLineEdit* const valueEdit = new QLineEdit(q);
};

EditPropertyDialog::EditPropertyDialog(QWidget* parent):
    base_type(parent),
    d(new Private{this})
{
    setWindowTitle("Edit Property");
    setResizeToContentsMode(Qt::Vertical);
    resize(400, 1);

    const auto nameLabel = new QLabel("Name", this);
    const auto valueLabel = new QLabel("Value", this);
    nameLabel->setBuddy(d->nameEdit);
    valueLabel->setBuddy(d->valueEdit);

    const auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    setButtonBox(buttons);

    const auto line = new QFrame(this);
    line->setFrameStyle(QFrame::HLine | QFrame::Sunken);

    const auto content = new QWidget(this);
    const auto grid = new QGridLayout(content);
    grid->setColumnMinimumWidth(0, style::Hints::kMinimumFormLabelWidth);
    grid->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);
    grid->setSpacing(style::Metrics::kStandardPadding);

    grid->addWidget(nameLabel, 0, 0, Qt::AlignRight);
    grid->addWidget(d->nameEdit, 0, 1);
    grid->addWidget(valueLabel, 1, 0, Qt::AlignRight);
    grid->addWidget(d->valueEdit, 1, 1);

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);

    layout->addWidget(content);
    layout->addWidget(line);
    layout->addWidget(buttons);

    connect(d->nameEdit, &QLineEdit::textChanged, this,
        [this]()
        {
            buttonBox()->button(QDialogButtonBox::Ok)->setEnabled(!propertyName().isEmpty());
        });
}

EditPropertyDialog::~EditPropertyDialog()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString EditPropertyDialog::propertyName() const
{
    return d->nameEdit->text().trimmed();
}

QString EditPropertyDialog::propertyValue() const
{
    return d->valueEdit->text();
}

} // namespace nx::vms::client::desktop
