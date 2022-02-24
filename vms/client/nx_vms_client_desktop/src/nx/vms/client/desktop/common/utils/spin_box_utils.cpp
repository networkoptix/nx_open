// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "spin_box_utils.h"

#include <QtCore/QSharedPointer>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionSpinBox>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>

#include <nx/utils/log/assert.h>

#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {
namespace spin_box_utils {

namespace {

static constexpr const char* kDefaultValuePropertyName = "__qn_spinBoxDefaultValue";

static const auto kDashDash = QString::fromWCharArray(L"\x2013\x2013");

template<class SpinBox>
using ValueType = std::decay_t<decltype(((SpinBox*)0)->value())>;

template<class SpinBox>
bool internalIsSpecialValue(SpinBox* spinBox)
{
    return !spinBox->specialValueText().isEmpty() && spinBox->value() == spinBox->minimum();
}

template<class SpinBox>
void internalSetSpecialValue(SpinBox* spinBox)
{
    if (spinBox->specialValueText().isEmpty())
    {
        NX_ASSERT(spinBox->minimum() - 1 < spinBox->minimum());
        spinBox->setMinimum(spinBox->minimum() - 1);
        spinBox->setSpecialValueText(kDashDash);
    }

    QSignalBlocker blocker(spinBox);
    spinBox->setValue(spinBox->minimum());
}

template<class SpinBox>
void internalSetDefaultValue(SpinBox* spinBox)
{
    const auto property = spinBox->property(kDefaultValuePropertyName);
    const auto defaultValue = property.template value<ValueType<SpinBox>>();
    spinBox->setValue(defaultValue);
    spinBox->selectAll();
}

// A class used for automatic setting of spin box default value when it's in special state
// and either up arrow is clicked with left mouse button or Up or PageUp key is pressed.
template<class SpinBox>
class SpinBoxDefaultValueSetter: public QObject
{
public:
    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (auto spinBox = qobject_cast<SpinBox*>(watched))
        {
            if (!internalIsSpecialValue(spinBox))
                return false;

            // Ignore input events for non-editable spinboxes.
            if (!spinBox->isEnabled() || spinBox->isReadOnly())
                return false;

            switch (event->type())
            {
                case QEvent::MouseButtonPress:
                case QEvent::MouseButtonDblClick:
                {
                    const auto mouseEvent = static_cast<QMouseEvent*>(event);
                    if (mouseEvent->button() != Qt::LeftButton)
                        break;

                    QStyleOptionSpinBox option;
                    option.initFrom(spinBox);

                    if (spinBox->style()->hitTestComplexControl(QStyle::CC_SpinBox, &option,
                        mouseEvent->pos(), spinBox) == QStyle::SC_SpinBoxUp)
                    {
                        internalSetDefaultValue(spinBox);
                        event->accept();
                        return true;
                    }

                    break;
                }

                case QEvent::Wheel:
                {
                    const auto wheelEvent = static_cast<QWheelEvent*>(event);
                    if (wheelEvent->angleDelta().y() > 0)
                    {
                        internalSetDefaultValue(spinBox);
                        event->accept();
                        return true;
                    }

                    break;
                }

                case QEvent::KeyPress:
                {
                    const auto keyEvent = static_cast<QKeyEvent*>(event);
                    switch (keyEvent->key())
                    {
                        case Qt::Key_Up:
                        case Qt::Key_PageUp:
                            internalSetDefaultValue(spinBox);
                            event->accept();
                            return true;

                        default:
                            break;
                    }

                    break;
                }

                default:
                    break;
            }
        }

        return false;
    }
};

template<class SpinBox>
void internalAutoClearSpecialValue(SpinBox* spinBox, ValueType<SpinBox> startingValue)
{
    static SpinBoxDefaultValueSetter<SpinBox> handler;
    spinBox->setProperty(kDefaultValuePropertyName, QVariant::fromValue(startingValue));
    spinBox->installEventFilter(&handler);

    using SpinBoxValueChanged = void (SpinBox::*)(ValueType<SpinBox>);
    const auto valueChanged = static_cast<SpinBoxValueChanged>(&SpinBox::valueChanged);

    spinBox->disconnect(&handler);
    QObject::connect(spinBox, valueChanged, &handler,
        [spinBox](ValueType<SpinBox> value)
        {
            if (value == spinBox->minimum() || spinBox->specialValueText().isEmpty())
                return;

            spinBox->setSpecialValueText(QString());
            spinBox->setMinimum(spinBox->minimum() + 1);
        });
}

} // namespace

void autoClearSpecialValue(QSpinBox* spinBox, int startingValue)
{
    internalAutoClearSpecialValue(spinBox, startingValue);
}

void setupSpinBox(QSpinBox* spinBox, bool sameValue, int value)
{
    if (sameValue)
        spinBox->setValue(value);
    else
        setSpecialValue(spinBox);
}

void setupSpinBox(QSpinBox* spinBox, std::optional<int> value)
{
    setupSpinBox(spinBox, bool(value), value.value_or(spinBox->value()));
}

void setupSpinBox(QSpinBox* spinBox, bool sameValue, int value, int min, int max)
{
    spinBox->setRange(min, max);
    setupSpinBox(spinBox, sameValue, value);
}

void setupSpinBox(QSpinBox* spinBox, std::optional<int> value, int min, int max)
{
    spinBox->setRange(min, max);
    setupSpinBox(spinBox, value);
}

std::optional<int> value(QSpinBox* spinBox)
{
    return isSpecialValue(spinBox) ? std::optional<int>() : spinBox->value();
}

void setSpecialValue(QSpinBox* spinBox)
{
    internalSetSpecialValue(spinBox);
}

bool isSpecialValue(QSpinBox* spinBox)
{
    return internalIsSpecialValue(spinBox);
}

void autoClearSpecialValue(QDoubleSpinBox* spinBox, qreal startingValue)
{
    internalAutoClearSpecialValue(spinBox, startingValue);
}

void setupSpinBox(QDoubleSpinBox* spinBox, bool sameValue, qreal value)
{
    if (sameValue)
        spinBox->setValue(value);
    else
        setSpecialValue(spinBox);
}

void setupSpinBox(QDoubleSpinBox* spinBox, std::optional<qreal> value)
{
    setupSpinBox(spinBox, bool(value), value.value_or(spinBox->value()));
}

void setupSpinBox(QDoubleSpinBox* spinBox, bool sameValue, qreal value, qreal min, qreal max)
{
    spinBox->setRange(min, max);
    setupSpinBox(spinBox, sameValue, value);
}

void setupSpinBox(QDoubleSpinBox* spinBox, std::optional<qreal> value, qreal min, qreal max)
{
    spinBox->setRange(min, max);
    setupSpinBox(spinBox, value);
}

std::optional<qreal> value(QDoubleSpinBox* spinBox)
{
    return isSpecialValue(spinBox) ? std::optional<qreal>() : spinBox->value();
}

void setSpecialValue(QDoubleSpinBox* spinBox)
{
    internalSetSpecialValue(spinBox);
}

bool isSpecialValue(QDoubleSpinBox* spinBox)
{
    return internalIsSpecialValue(spinBox);
}

} // namespace spin_box_utils
} // namespace nx::vms::client::desktop
