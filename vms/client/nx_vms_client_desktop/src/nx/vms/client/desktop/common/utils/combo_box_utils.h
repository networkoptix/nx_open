// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QVariant>

class QComboBox;
class QString;
class QIcon;

namespace nx::vms::client::desktop {
namespace combo_box_utils {

void setItemHidden(QComboBox* combo, int index, bool hidden = true);

void addHiddenItem(QComboBox* combo, const QString& text, const QVariant& data = QVariant());

void addHiddenItem(QComboBox* combo, const QIcon& icon, const QString& text,
    const QVariant& data = QVariant());

void insertHiddenItem(QComboBox* combo, int index, const QString& text,
    const QVariant& data = QVariant());

void insertHiddenItem(QComboBox* combo, int index, const QIcon& icon,
    const QString& text, const QVariant& data = QVariant());

QString multipleValuesText();
void insertMultipleValuesItem(QComboBox* combo, int index = 0);

/**
 * Makes Enter/Return pressed in the open combo box popup select the current item without
 * triggering the default button of the dialog which contains the combo box (QTBUG-121023). MacOS
 * specific.
 */
void installPopupEnterKeyHandlerIfRequired(QComboBox* combo);

} // namespace combo_box_utils
} // namespace nx::vms::client::desktop
