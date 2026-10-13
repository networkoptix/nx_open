// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "combo_box_utils.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>

#include <nx/utils/log/assert.h>

namespace {

struct ComboBoxUtils
{
    Q_DECLARE_TR_FUNCTIONS(ComboBoxUtils)
};

#ifdef Q_OS_MACOS

/**
 * Event filter for the combo box popup view. Postpones the item selection by the Enter key from
 * the ShortcutOverride phase to the KeyPress phase.
 */
class PopupEnterKeyHandler: public QObject
{
public:
    explicit PopupEnterKeyHandler(QComboBox* combo): QObject(combo), m_combo(combo) {}

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        const auto type = event->type();
        if (type != QEvent::ShortcutOverride && type != QEvent::KeyPress)
            return false;

        const auto key = static_cast<QKeyEvent*>(event)->key();
        if (key != Qt::Key_Return && key != Qt::Key_Enter)
            return false;

        const auto view = m_combo->view();
        if (watched != view || !view->isVisible())
            return false;

        const auto index = view->currentIndex();
        if (index.parent() != m_combo->rootModelIndex())
            return false; //< Nested items are not supported, let Qt handle the key.

        const bool selectable = index.isValid() && index.flags().testFlag(Qt::ItemIsEnabled);
        if (type == QEvent::KeyPress && selectable)
        {
            // Replicates what Qt does on the item selection from the popup.
            const int row = index.row();
            const QString text = m_combo->itemText(row);
            m_combo->hidePopup();
            if (row != m_combo->currentIndex())
            {
                m_combo->setCurrentIndex(row);
            }
            else if (const auto lineEdit = m_combo->lineEdit())
            {
                lineEdit->selectAll();
                lineEdit->setText(text);
            }
            emit m_combo->activated(row);
            emit m_combo->textActivated(text);
        }

        event->accept();
        return true;
    }

private:
    QComboBox* const m_combo;
};

#endif // Q_OS_MACOS

} // namespace

namespace nx::vms::client::desktop {
namespace combo_box_utils {

void setItemHidden(QComboBox* combo, int index, bool hidden)
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
        NX_ASSERT(false, "combo_box_utils::setItemHidden doesn't support non-standard item views.");

    if (auto model = qobject_cast<QStandardItemModel*>(combo->model()))
    {
        static const auto kDefaultFlags = QStandardItem().flags();
        model->item(index)->setFlags(hidden ? Qt::NoItemFlags : kDefaultFlags);
    }
    else
    {
        NX_ASSERT(false,
            "combo_box_utils::setItemHidden supports only combo boxes with QStandardItemModel.");
    }
}

void addHiddenItem(QComboBox* combo, const QString& text, const QVariant& data)
{
    addHiddenItem(combo, QIcon(), text, data);
}

void addHiddenItem(
    QComboBox* combo, const QIcon& /*icon*/, const QString& text, const QVariant& data)
{
    NX_ASSERT(combo);
    insertHiddenItem(combo, combo->count(), QIcon(), text, data);
}

void insertHiddenItem(
    QComboBox* combo, int index, const QString& text, const QVariant& data)
{
    insertHiddenItem(combo, index, QIcon(), text, data);
}

void insertHiddenItem(
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

QString multipleValuesText()
{
    return QString("<%1>").arg(ComboBoxUtils::tr("multiple values"));
}

void insertMultipleValuesItem(QComboBox* combo, int index)
{
    insertHiddenItem(combo, index, multipleValuesText());
}

void installPopupEnterKeyHandlerIfRequired(QComboBox* combo)
{
#ifdef Q_OS_MACOS
    if (!NX_ASSERT(combo))
        return;

    combo->view()->installEventFilter(new PopupEnterKeyHandler(combo));
#else
    Q_UNUSED(combo);
#endif
}

} // namespace combo_box_utils
} // namespace nx::vms::client::desktop
