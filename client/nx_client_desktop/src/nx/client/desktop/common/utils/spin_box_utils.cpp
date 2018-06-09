#include "spin_box_utils.h"

#include <QtCore/QSharedPointer>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLineEdit>

#include <ui/workaround/widgets_signals_workaround.h>

namespace nx {
namespace client {
namespace desktop {

void SpinBoxUtils::autoClearSpecialValue(QSpinBox* spinBox, int startingValue)
{
    static QObject handler;

    spinBox->disconnect(&handler);

    QObject::connect(spinBox, QnSpinboxIntValueChanged, &handler,
        [spinBox, startingValue]()
        {
            if (spinBox->specialValueText().isEmpty())
                return;

            spinBox->setSpecialValueText(QString());
            spinBox->setMinimum(spinBox->minimum() + 1);
            spinBox->setValue(startingValue);
        });
}

void SpinBoxUtils::setupSpinBox(QSpinBox* spinBox, bool sameValue, int value)
{
    if (sameValue)
        spinBox->setValue(value);
    else
        setSpecialValue(spinBox);
}

void SpinBoxUtils::setupSpinBox(QSpinBox* spinBox, std::optional<int> value)
{
    setupSpinBox(spinBox, bool(value), value.value_or(spinBox->value()));
}

void SpinBoxUtils::setupSpinBox(QSpinBox* spinBox, bool sameValue, int value, int min, int max)
{
    spinBox->setRange(min, max);
    setupSpinBox(spinBox, sameValue, value);
}

void SpinBoxUtils::setupSpinBox(QSpinBox* spinBox, std::optional<int> value, int min, int max)
{
    spinBox->setRange(min, max);
    setupSpinBox(spinBox, value);
}

std::optional<int> SpinBoxUtils::value(QSpinBox* spinBox)
{
    return isSpecialValue(spinBox) ? std::optional<int>() : spinBox->value();
}

void SpinBoxUtils::setSpecialValue(QSpinBox* spinBox)
{
    if (spinBox->specialValueText().isEmpty())
    {
        NX_ASSERT(spinBox->minimum() - 1 < spinBox->minimum());
        spinBox->setMinimum(spinBox->minimum() - 1);
        spinBox->setSpecialValueText(QString::fromWCharArray(L"\x2013\x2013"));
    }

    QSignalBlocker blocker(spinBox);
    spinBox->setValue(spinBox->minimum());
}

bool SpinBoxUtils::isSpecialValue(QSpinBox* spinBox)
{
    return !spinBox->specialValueText().isEmpty() && spinBox->value() == spinBox->minimum();
}

} // namespace desktop
} // namespace client
} // namespace nx
