#include "combo_box_field.h"

#include <QtCore/QEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QListView>

#include <ui/common/accessor.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/math/math.h>

namespace {

Qn::TextValidateFunction initializationValidator()
{
    const auto validator =
        [](const QString& /*text*/)
        {
            return Qn::ValidationResult(QValidator::Intermediate);
        };

    return validator;
}

}

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ComboBoxField* ComboBoxField::create(
    const QStringList& items,
    int currentIndex,
    const Qn::TextValidateFunction& validator,
    QWidget* parent)
{
    return new ComboBoxField(items, currentIndex, validator, parent);
}

ComboBoxField::ComboBoxField(
    const QStringList& items,
    int currentIndex,
    const Qn::TextValidateFunction& validator,
    QWidget* parent)
    :
    ComboBoxField(parent)
{
    setValidator(initializationValidator());
    setItems(items);
    setCurrentIndex(currentIndex);
    setValidator(validator);

    initialize();
}

ComboBoxField::ComboBoxField(QWidget* parent):
    base_type(new QComboBox(),
        PropertyAccessor::create("currentText"),
        PropertyAccessor::create("readOnly"),
        AccessorPtr(),
        false,
        parent)
{
    initialize();
}

void ComboBoxField::initialize()
{
    const auto handleTextChanged =
        [this]()
        {
            emit textChanged(text());
            validate();
        };

    const auto control = combobox();
    connect(control, &QComboBox::editTextChanged, this, handleTextChanged);
    connect(control, QnComboboxCurrentIndexChanged, this,
        [this, control, handleTextChanged](int /*index*/)
        {
            handleTextChanged();
            emit currentIndexChanged();
        });
    connect(control, QnComboboxActivated, this,
        [this](int /*index*/)
        {
            validate();
        });
}

bool ComboBoxField::eventFilter(QObject *object, QEvent *event)
{
    const auto control = combobox();
    if (object != control || event->type() != QEvent::MouseButtonPress)
        return base_type::eventFilter(object, event);

    if (!control->hasFocus())
        control->setFocus();

    return false;
}

QComboBox* ComboBoxField::combobox()
{
    return qobject_cast<QComboBox*>(input());
}

const QComboBox* ComboBoxField::combobox() const
{
    return qobject_cast<QComboBox*>(input());
}

void ComboBoxField::setItems(const QStringList& items)
{
    const auto control = combobox();
    control->clear();
    control->addItems(items);
}

QStringList ComboBoxField::items() const
{
    QStringList result;
    const auto control = combobox();
    const auto count = control->count();
    for (int i = 0; i != count; ++i)
        result.append(control->itemText(i));

    return result;
}

bool ComboBoxField::setRowHidden(int index, bool value)
{
    const auto listView = qobject_cast<QListView*>(combobox()->view());
    if (!listView)
    {
        NX_EXPECT(false, "Invalid item view delegate");
        return false;
    }

    if (!qBetween(0, index, combobox()->count()))
    {
        NX_EXPECT(false, "Invalid index value");
        return false;
    }

    listView->setRowHidden(index, value);
    return true;
}

void ComboBoxField::setCurrentIndex(int index)
{
    combobox()->setCurrentIndex(index);
}

int ComboBoxField::currentIndex() const
{
    return combobox()->currentIndex();
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

