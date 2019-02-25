#include "spin_box_utils.h"

#include <QtCore/QSharedPointer>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionSpinBox>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>

#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {
namespace spin_box_utils {

namespace {

static constexpr const char* kDefaultValuePropertyName = "__qn_spinBoxDefaultValue";

// A class used for automatic setting of spin box default value when it's in special state
// and either up arrow is clicked with left mouse button or Up or PageUp key is pressed.
class SpinBoxDefaultValueSetter: public QObject
{
public:
    static void setDefaultValue(QSpinBox* spinBox)
    {
        const int defaultValue = spinBox->property(kDefaultValuePropertyName).toInt();
        spinBox->setValue(defaultValue);
        spinBox->selectAll();
    }

    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (auto spinBox = qobject_cast<QSpinBox*>(watched))
        {
            if (!isSpecialValue(spinBox))
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
                        setDefaultValue(spinBox);
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
                        setDefaultValue(spinBox);
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
                            setDefaultValue(spinBox);
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

} // namespace

void autoClearSpecialValue(QSpinBox* spinBox, int startingValue)
{
    static SpinBoxDefaultValueSetter handler;
    spinBox->setProperty(kDefaultValuePropertyName, startingValue);
    spinBox->installEventFilter(&handler);

    spinBox->disconnect(&handler);
    QObject::connect(spinBox, QnSpinboxIntValueChanged, &handler,
        [spinBox](int value)
        {
            if (value == spinBox->minimum() || spinBox->specialValueText().isEmpty())
                return;

            spinBox->setSpecialValueText(QString());
            spinBox->setMinimum(spinBox->minimum() + 1);
        });
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
    if (spinBox->specialValueText().isEmpty())
    {
        NX_ASSERT(spinBox->minimum() - 1 < spinBox->minimum());
        spinBox->setMinimum(spinBox->minimum() - 1);
        spinBox->setSpecialValueText(QString::fromWCharArray(L"\x2013\x2013"));
    }

    QSignalBlocker blocker(spinBox);
    spinBox->setValue(spinBox->minimum());
}

bool isSpecialValue(QSpinBox* spinBox)
{
    return !spinBox->specialValueText().isEmpty() && spinBox->value() == spinBox->minimum();
}

} // namespace spin_box_utils
} // namespace nx::vms::client::desktop
