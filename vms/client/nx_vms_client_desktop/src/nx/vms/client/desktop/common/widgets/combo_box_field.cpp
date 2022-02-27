// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "combo_box_field.h"

#include <QtCore/QEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QListView>

#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/math/math.h>

namespace {

static const auto kCurrentTextPropertyName = QByteArray("currentText");
static const auto kReadOnlyPropertyName = QByteArray("readOnly");

} // namespace

namespace nx::vms::client::desktop {

ComboBoxField* ComboBoxField::create(
    const QStringList& items,
    int currentIndex,
    const TextValidateFunction& validator,
    QWidget* parent)
{
    return new ComboBoxField(items, currentIndex, validator, parent);
}

ComboBoxField::ComboBoxField(
    const QStringList& items,
    int currentIndex,
    const TextValidateFunction& validator,
    QWidget* parent)
    :
    base_type(new QComboBox(),
        PropertyAccessor::create(kCurrentTextPropertyName),
        PropertyAccessor::create(kReadOnlyPropertyName),
        /* placeholderAccessor */ AccessorPtr(),
        /* useWarningStyleForControl */ false,
        ValidationBehavior::validateOnInputShowOnFocus,
        parent)
{
    setItems(items);
    setCurrentIndex(currentIndex);
    setValidator(validator);
    initialize();
}

ComboBoxField::ComboBoxField(QWidget* parent):
    base_type(new QComboBox(),
        PropertyAccessor::create(kCurrentTextPropertyName),
        PropertyAccessor::create(kReadOnlyPropertyName),
        /* placeholderAccessor */ AccessorPtr(),
        /* useWarningStyleForControl */ false,
        ValidationBehavior::validateOnInputShowOnFocus,
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
        [this, handleTextChanged](int /*index*/)
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
        NX_ASSERT(false, "Invalid item view delegate");
        return false;
    }

    if (!qBetween(0, index, combobox()->count()))
    {
        NX_ASSERT(false, "Invalid index value");
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

} // namespace nx::vms::client::desktop

