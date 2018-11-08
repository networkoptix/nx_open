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

} // namespace combo_box_utils
} // namespace nx::vms::client::desktop
