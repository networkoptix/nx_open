#include "checkbox_utils.h"

#include <QtWidgets/QCheckBox>

void QnCheckboxUtils::autoClearTristate(QCheckBox* checkbox)
{
    const auto clearTristate =
        [checkbox]()
        {
            checkbox->setTristate(false);

            if (checkbox->checkState() == Qt::PartiallyChecked)
                checkbox->setCheckState(Qt::Checked);
        };

    QObject::connect(checkbox,  &QCheckBox::clicked, checkbox,
        clearTristate, Qt::DirectConnection);
}

void QnCheckboxUtils::setupTristateCheckbox(QCheckBox* checkbox, bool sameValue, bool checked)
{
    checkbox->setTristate(!sameValue);
    if (!sameValue)
        checkbox->setCheckState(Qt::PartiallyChecked);
    else
        checkbox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}
