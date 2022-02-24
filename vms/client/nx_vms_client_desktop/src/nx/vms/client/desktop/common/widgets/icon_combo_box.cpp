// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "icon_combo_box.h"

#include <QtCore/QtMath>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QListView>

#include <ui/common/indents.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>

#include <nx/vms/client/desktop/common/delegates/styled_combo_box_delegate.h>

namespace {

static constexpr auto kIconNameRole = Qt::AccessibleTextRole;

} // namespace

namespace nx::vms::client::desktop {

class IconComboBox::Delegate: public StyledComboBoxDelegate
{
public:
    using StyledComboBoxDelegate::StyledComboBoxDelegate; //< Forward constructors.

    virtual QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        if (!m_itemSize.isEmpty())
            return m_itemSize;

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        return qnSkin->maximumSize(opt.icon);
    }

    QSize m_itemSize;
};

IconComboBox::IconComboBox(QWidget* parent) :
    base_type(parent),
    m_delegate(new Delegate())
{
    auto listView = new QListView(this);
    listView->setItemDelegate(m_delegate.data());
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

IconComboBox::~IconComboBox()
{
}

void IconComboBox::setIcons(const QString& path,
    const QStringList& names, const QString& extension)
{
    const QString ext = extension.startsWith('.')
        ? extension
        : lit(".") + extension;

    QVector<QPair<QString, QIcon>> icons;
    icons.reserve(names.size());

    for (const auto& name: names)
    {
        const auto fullName = path + lit("/") + name + ext;
        const auto icon = qnSkin->icon(fullName);
        if (!icon.isNull())
            icons.push_back({ name, icon });
    }

    setIcons(icons);
}

void IconComboBox::setPixmaps(const QString& path,
    const QStringList& names, const QString& extension)
{
    const QString ext = extension.startsWith('.')
        ? extension
        : lit(".") + extension;

    QVector<QPair<QString, QPixmap>> pixmaps;
    pixmaps.reserve(names.size());

    for (const auto& name : names)
    {
        const auto fullName = path + lit("/") + name + ext;
        const auto pixmap = qnSkin->pixmap(fullName);
        if (!pixmap.isNull())
            pixmaps.push_back({ name, pixmap });
    }

    setPixmaps(pixmaps);
}

void IconComboBox::setIcons(const QVector<QPair<QString, QIcon>>& icons)
{
    auto model = new QStandardItemModel(icons.size(), 1, this);

    int row = 0;
    QSize iconSize;

    for (const auto& icon: icons)
    {
        auto item = new QStandardItem(icon.second, QString());
        iconSize = iconSize.expandedTo(qnSkin->maximumSize(icon.second));
        item->setData(icon.first, kIconNameRole);
        model->setItem(row++, item);
    }

    setIconSize(iconSize);
    setModel(model); //< Will delete old model if it was owned.
}

void IconComboBox::setPixmaps(const QVector<QPair<QString, QPixmap>>& pixmaps)
{
    QVector<QPair<QString, QIcon>> icons;
    icons.reserve(pixmaps.size());

    for (const auto& pixmap: pixmaps)
    {
        QIcon icon;
        icon.addPixmap(pixmap.second, QIcon::Normal);
        icon.addPixmap(pixmap.second, QIcon::Active);
        icon.addPixmap(pixmap.second, QIcon::Selected);
        icons.push_back({ pixmap.first, icon });
    }

    setIcons(icons);
}

int IconComboBox::columnCount() const
{
    return m_columnCount;
}

void IconComboBox::setColumnCount(int count)
{
    m_columnCount = count;
}

int IconComboBox::maxVisibleRows() const
{
    return m_maxVisibleRows;
}

void IconComboBox::setMaxVisibleRows(int count)
{
    m_maxVisibleRows = count;
}

QSize IconComboBox::itemSize() const
{
    return m_delegate->m_itemSize;
}

void IconComboBox::setItemSize(const QSize& size)
{
    m_delegate->m_itemSize = size;
}

QString IconComboBox::currentIcon() const
{
    return currentData(kIconNameRole).toString();
}

void IconComboBox::setCurrentIcon(const QString& name)
{
    const auto indices = model()->match(model()->index(0, 0), kIconNameRole, name);
    if (!indices.empty())
        setCurrentIndex(indices.front().row());
}

void IconComboBox::adjustPopupParameters()
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

    const auto gridSize = m_delegate->m_itemSize.isEmpty()
        ? iconSize()
        : m_delegate->m_itemSize;

    listView->setIconSize(iconSize());
    listView->setGridSize(gridSize);

    setMaxVisibleItems(qMin(m_maxVisibleRows, rows));

    const bool showScrollBar = rows > m_maxVisibleRows;
    const int scrollBarExtra = showScrollBar
        ? listView->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this)
        : 0;

    // We must explicitly switch scroll bar policy or items will be incorrectly laid out.
    listView->setVerticalScrollBarPolicy(showScrollBar
        ? Qt::ScrollBarAlwaysOn
        : Qt::ScrollBarAlwaysOff);

    const auto margins = listView->parentWidget()->contentsMargins();
    const int popupWidth = gridSize.width() * columns + scrollBarExtra
        + margins.left() + margins.right() + 1; //< Without "+1" items lay out incorrectly.

    setProperty(style::Properties::kComboBoxPopupWidth, popupWidth);
}

void IconComboBox::showPopup()
{
    adjustPopupParameters();
    base_type::showPopup();
}

} // namespace nx::vms::client::desktop
