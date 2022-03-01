// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enabled_state_item_delegate.h"

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/style/style.h>

namespace nx::vms::client::desktop::rules {

EnabledStateItemDelegate::EnabledStateItemDelegate(QObject* parent):
    SwitchItemDelegate(parent)
{
}

QWidget* EnabledStateItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    auto editor = new QPushButton(parent);
    editor->setCheckable(true);
    connect(editor, &QPushButton::toggled, this, &EnabledStateItemDelegate::commitAndClose);

    return editor;
}

void EnabledStateItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto checkBoxEditor = static_cast<QPushButton*>(editor);
    checkBoxEditor->setChecked(
        index.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked);
}

void EnabledStateItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
    const QModelIndex& index) const
{
    auto checkBoxEditor = static_cast<QPushButton*>(editor);
    model->setData(
        index, checkBoxEditor->isChecked() ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
}

void EnabledStateItemDelegate::updateEditorGeometry(QWidget* editor,
    const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    editor->setGeometry(option.rect);
}

void EnabledStateItemDelegate::commitAndClose()
{
    auto editor = qobject_cast<QPushButton*>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}

} // namespace nx::vms::client::desktop:rules
