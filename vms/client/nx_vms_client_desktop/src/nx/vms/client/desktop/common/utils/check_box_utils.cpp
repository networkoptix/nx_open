#include "check_box_utils.h"

#include <QtWidgets/QCheckBox>

namespace nx::vms::client::desktop {
namespace check_box_utils {

void autoClearTristate(QCheckBox* checkbox)
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

void setupTristateCheckbox(QCheckBox* checkbox, bool sameValue, bool checked)
{
    checkbox->setTristate(!sameValue);
    if (!sameValue)
        checkbox->setCheckState(Qt::PartiallyChecked);
    else
        checkbox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

void setupTristateCheckbox(QCheckBox* checkbox, std::optional<bool> checked)
{
    setupTristateCheckbox(checkbox, bool(checked), checked.value_or(false));
}

} // namespace check_box_utils
} // namespace nx::vms::client::desktop
