#pragma once

#include <QtCore/QString>
#include <QtCore/QVariant>

class QComboBox;
class QString;
class QIcon;

namespace nx {
namespace client {
namespace desktop {

struct ComboBoxUtils
{
    static void setItemHidden(QComboBox* combo, int index, bool hidden = true);

    static void addHiddenItem(QComboBox* combo, const QString& text,
        const QVariant& data = QVariant());

    static void addHiddenItem(QComboBox* combo, const QIcon& icon, const QString& text,
        const QVariant& data = QVariant());

    static void insertHiddenItem(QComboBox* combo, int index, const QString& text,
        const QVariant& data = QVariant());

    static void insertHiddenItem(QComboBox* combo, int index, const QIcon& icon,
        const QString& text, const QVariant& data = QVariant());

    static QString multipleValuesText();
    static void insertMultipleValuesItem(QComboBox* combo, int index = 0);
};

} // namespace desktop
} // namespace client
} // namespace nx
