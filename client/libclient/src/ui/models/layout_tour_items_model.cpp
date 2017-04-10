#include "layout_tour_items_model.h"

#include <core/resource/layout_resource.h>
#include <core/resource/layout_tour_item.h>

#include <nx/utils/log/assert.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>

#include <utils/common/qtimespan.h>
#include <utils/math/math.h>

QnLayoutTourItemsModel::QnLayoutTourItemsModel(QObject* parent):
    base_type(parent)
{

}

QnLayoutTourItemsModel::~QnLayoutTourItemsModel()
{

}

int QnLayoutTourItemsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : (int)m_items.size();
}

int QnLayoutTourItemsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return ColumnCount;
}

QVariant QnLayoutTourItemsModel::data(const QModelIndex& index, int role) const
{
    //TODO: #GDM #3.1 get common place for this check
    if (index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const bool hasItem = qBetween(0, index.row(), (int)m_items.size());
    NX_EXPECT(hasItem);
    if (!hasItem)
        return QVariant();

    Column column = static_cast<Column>(index.column());
    QnLayoutTourItem item = m_items[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleDescriptionRole:
        case Qt::ToolTipRole:
        {
            switch (column)
            {
                case OrderColumn:
                    return QString::number(index.row());
                case LayoutColumn:
                    //TODO: #GDM #3.1 move to delegate if will support this dialog
                    return item.layout ? item.layout->getName() : QVariant();
                case DelayColumn:
                    return QTimeSpan(item.delayMs).toApproximateString(
                        QTimeSpan::kDoNotSuppressSecondUnit,
                        Qt::Seconds,
                        QTimeSpan::SuffixFormat::Short);
                case ControlsColumn:
                    return tr("Remove from tour");
                default:
                    return QVariant();
            }
        }

        case Qt::DecorationRole:
        {
            //TODO: #GDM #3.1 move to delegate if will support this dialog
            switch (column)
            {
                case QnLayoutTourItemsModel::LayoutColumn:
                    if (item.layout)
                        return qnResIconCache->icon(item.layout);
                    return QVariant();

                case QnLayoutTourItemsModel::MoveUpColumn:
                    return qnSkin->pixmap("buttons/up.png");

                case QnLayoutTourItemsModel::MoveDownColumn:
                    return qnSkin->pixmap("buttons/down.png");

                default:
                    break;
            }

            return QVariant();
        }

        case Qt::TextAlignmentRole:
        {
            //TODO: #GDM #3.1 move to delegate if will support this dialog
            switch (column)
            {
                case QnLayoutTourItemsModel::ControlsColumn:
                    return Qt::AlignRight;

                default:
                    break;
            }

            return QVariant();
        }

        case Qt::EditRole:
            switch (column)
            {
                case QnLayoutTourItemsModel::DelayColumn:
                    return item.delayMs;

                default:
                    break;
            }

            return QVariant();

        default:
            break;
    }

    return QVariant();
}

QVariant QnLayoutTourItemsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation == Qt::Vertical)
        return base_type::headerData(section, orientation, role);

    switch (section)
    {
        case OrderColumn:
            return lit("#");

        case LayoutColumn:
            return tr("Layout");

        case DelayColumn:
            return tr("Display for");

        default:
            break;
    }

    return QVariant();
}

Qt::ItemFlags QnLayoutTourItemsModel::flags(const QModelIndex &index) const
{
    auto result = base_type::flags(index);
    if (index.column() == DelayColumn)
        result |= Qt::ItemIsEditable;
    return result;
}

bool QnLayoutTourItemsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole)
        return false;

    if (index.column() != DelayColumn)
        return false;

    if (value.toInt() <= 0)
        return false;

    const bool hasItem = qBetween(0, index.row(), (int)m_items.size());
    NX_EXPECT(hasItem);
    if (!hasItem)
        return false;

    auto& item = m_items[index.row()];
    item.delayMs = value.toInt();
    return true;
}

void QnLayoutTourItemsModel::reset(const QnLayoutTourItemList& items)
{
    ScopedReset guard(this);
    m_items = items;
}

void QnLayoutTourItemsModel::addItem(const QnLayoutTourItem& item)
{
    const int count = (int)m_items.size();
    ScopedInsertRows guard(this, QModelIndex(), count, count);
    m_items.push_back(item);
}

void QnLayoutTourItemsModel::updateLayout(int row, const QnLayoutResourcePtr& layout)
{
    auto& item = m_items[row];
    if (item.layout == layout)
        return;

    item.layout = layout;
    emit dataChanged(index(row), index(row, ControlsColumn - 1));
}

void QnLayoutTourItemsModel::moveUp(int row)
{
    if (row <= 0)
        return;

    ScopedMoveRows guard(this, QModelIndex(), row, row, QModelIndex(), row - 1);
    std::swap(m_items[row - 1], m_items[row]);
}

void QnLayoutTourItemsModel::moveDown(int row)
{
    if (row >= m_items.size() - 1)
        return;
    moveUp(row + 1);
}

void QnLayoutTourItemsModel::removeItem(int row)
{
    ScopedRemoveRows guard(this, QModelIndex(), row, row);
    m_items.erase(m_items.begin() + row);
}

QnLayoutTourItemList QnLayoutTourItemsModel::items() const
{
    return m_items;
}
