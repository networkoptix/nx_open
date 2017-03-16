#include "icon_selection_combo_box.h"

#include <QtGui/QStandardItemModel>

#include <ui/common/indents.h>
#include <ui/delegates/styled_combo_box_delegate.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>

namespace {

static constexpr auto kIconNameRole = Qt::AccessibleTextRole;

} // namespace

QnIconSelectionComboBox::QnIconSelectionComboBox(QWidget* parent) :
    base_type(parent)
{
    auto listView = new QListView(this);
    listView->setItemDelegate(new QnStyledComboBoxDelegate());
    listView->setViewMode(QListView::IconMode);
    listView->setWrapping(true);
    listView->setResizeMode(QListView::Adjust);
    listView->setUniformItemSizes(true);
    listView->setAutoFillBackground(false);
    listView->viewport()->setAutoFillBackground(false);
    listView->setProperty(style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents(0, 0)));
    setView(listView);
}

QnIconSelectionComboBox::~QnIconSelectionComboBox()
{
}

void QnIconSelectionComboBox::setIcons(const QString& path,
    const QStringList& names, const QString& extension)
{
    const QString ext = extension.startsWith(L'.')
        ? extension
        : lit(".") + extension;

    auto model = new QStandardItemModel(names.size(), 1, this);

    int row = 0;
    QSize iconSize;

    for (const auto& name: names)
    {
        const auto fullName = path + lit("/") + name + ext;
        const auto icon = qnSkin->icon(fullName);
        auto item = new QStandardItem(icon, QString());
        iconSize = iconSize.expandedTo(qnSkin->maximumSize(icon));
        item->setData(name, kIconNameRole);
        model->setItem(row++, item);
    }

    setIconSize(iconSize);
    setModel(model); //< will delete old model if it was owned
}

int QnIconSelectionComboBox::columnCount() const
{
    return m_columnCount;
}

void QnIconSelectionComboBox::setColumnCount(int count)
{
    m_columnCount = count;
}

int QnIconSelectionComboBox::maxRowCount() const
{
    return m_maxRowCount;
}

void QnIconSelectionComboBox::setMaxRowCount(int count)
{
    m_maxRowCount = count;
}

QString QnIconSelectionComboBox::currentIcon() const
{
    return currentData(kIconNameRole).toString();
}

void QnIconSelectionComboBox::setCurrentIcon(const QString& name)
{
    const auto indices = model()->match(model()->index(0, 0), kIconNameRole, name);
    if (!indices.empty())
        setCurrentIndex(indices.front().row());
}

void QnIconSelectionComboBox::adjustPopupParameters()
{
    auto listView = qobject_cast<QListView*>(view());
    if (!listView || listView->viewMode() != QListView::IconMode)
        return;

    const int count = model()->rowCount();
    if (count == 0)
        return;

    const int effectiveColumnCount = m_columnCount
        ? m_columnCount
        : qCeil(qSqrt(count));

    const int columns = qMin(count, effectiveColumnCount);
    const int rows = qCeil(static_cast<qreal>(count) / columns);

    listView->setIconSize(iconSize());
    listView->setGridSize(iconSize());

    setMaxVisibleItems(qMin(m_maxRowCount, rows));

    const bool showScrollBar = rows > m_maxRowCount;
    const int scrollBarExtra = showScrollBar
        ? listView->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this)
        : 0;

    /* We must explicitly switch scroll bar policy or items will be incorrectly laid out. */
    listView->setVerticalScrollBarPolicy(showScrollBar
        ? Qt::ScrollBarAlwaysOn
        : Qt::ScrollBarAlwaysOff);

    const auto margins = listView->parentWidget()->contentsMargins();
    const int popupWidth = iconSize().width() * columns + scrollBarExtra
        + margins.left() + margins.right() + 1; //< without "+1" items lay out incorrectly.

    setProperty(style::Properties::kComboBoxPopupWidth, popupWidth);
}

void QnIconSelectionComboBox::showPopup()
{
    adjustPopupParameters();
    base_type::showPopup();
}
