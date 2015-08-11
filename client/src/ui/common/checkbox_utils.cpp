#include "checkbox_utils.h"

#include <QtWidgets/QCheckBox>

QnCheckbox::QnCheckbox(QObject* parent):
    QObject(parent)
{
}

QnCheckbox::~QnCheckbox()
{
}

void QnCheckbox::autoCleanTristate(QCheckBox* checkbox)
{
    QnCheckbox* helper = new QnCheckbox(checkbox);
    connect(checkbox,  &QCheckBox::clicked, helper, &QnCheckbox::cleanTristate);
}

void QnCheckbox::cleanTristate() {
    auto checkbox = qobject_cast<QCheckBox*>(sender());
    Q_ASSERT_X(checkbox, Q_FUNC_INFO, "Checkbox must be present");
    if (!checkbox)
        return;

    Qt::CheckState state = checkbox->checkState();

    checkbox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        checkbox->setCheckState(Qt::Checked);
}

void QnCheckbox::setupTristateCheckbox(QCheckBox* checkbox, bool sameValue, bool checked) {
    checkbox->setTristate(!sameValue);
    if (!sameValue)
        checkbox->setCheckState(Qt::PartiallyChecked);
    else
        checkbox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}
