#include "combo_box_utils.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QListView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QTableView>

#include <nx/utils/log/assert.h>

namespace {

struct ComboBoxUtilsContext
{
    Q_DECLARE_TR_FUNCTIONS(ComboBoxUtils)
};

} // namespace

namespace nx {
namespace client {
namespace desktop {

void ComboBoxUtils::setItemHidden(QComboBox* combo, int index, bool hidden)
{
    NX_ASSERT(combo);
    NX_ASSERT(index >= 0 && index < combo->count());

    if (auto list = qobject_cast<QListView*>(combo->view()))
        list->setRowHidden(index, hidden);
    else if (auto tree = qobject_cast<QTreeView*>(combo->view()))
        tree->setRowHidden(index, combo->rootModelIndex(), hidden);
    else if (auto table = qobject_cast<QTableView*>(combo->view()))
        table->setRowHidden(index, hidden);
    else
        NX_ASSERT(false, "ComboBoxUtils::setItemHidden doesn't support non-standard item views.");

    if (auto model = qobject_cast<QStandardItemModel*>(combo->model()))
    {
        static const auto kDefaultFlags = QStandardItem().flags();
        model->item(index)->setFlags(hidden ? Qt::NoItemFlags : kDefaultFlags);
    }
    else
    {
        NX_ASSERT(false,
            "ComboBoxUtils::setItemHidden supports only combo boxes with QStandardItemModel.");
    }
}

void ComboBoxUtils::addHiddenItem(QComboBox* combo, const QString& text, const QVariant& data)
{
    addHiddenItem(combo, QIcon(), text, data);
}

void ComboBoxUtils::addHiddenItem(
    QComboBox* combo, const QIcon& icon, const QString& text, const QVariant& data)
{
    NX_ASSERT(combo);
    insertHiddenItem(combo, combo->count(), QIcon(), text, data);
}

void ComboBoxUtils::insertHiddenItem(
    QComboBox* combo, int index, const QString& text, const QVariant& data)
{
    insertHiddenItem(combo, index, QIcon(), text, data);
}

void ComboBoxUtils::insertHiddenItem(
    QComboBox* combo,
    int index,
    const QIcon& icon,
    const QString& text,
    const QVariant& data)
{
    NX_ASSERT(combo && combo->count() < combo->maxCount());
    NX_ASSERT(index >= 0 && index <= combo->count());
    const int oldCount = combo->count();
    combo->insertItem(index, icon, text, data);
    NX_ASSERT(oldCount + 1 == combo->count());
    setItemHidden(combo, index);
}

QString ComboBoxUtils::multipleValuesText()
{
    return lit("<%1>").arg(ComboBoxUtilsContext::tr("multiple values"));
}

void ComboBoxUtils::insertMultipleValuesItem(QComboBox* combo, int index)
{
    insertHiddenItem(combo, index, multipleValuesText());
}

} // namespace desktop
} // namespace client
} // namespace nx
