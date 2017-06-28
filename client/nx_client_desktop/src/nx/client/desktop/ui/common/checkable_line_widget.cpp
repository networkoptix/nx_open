#include "checkable_line_widget.h"

#include <functional>

#include <QtCore/QAbstractItemModel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHeaderView>

#include <client/client_globals.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/tree_view.h>

namespace {

QSize sizeOf(const QMargins& m)
{
    return QSize(m.left() + m.right(), m.top() + m.bottom());
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

//-------------------------------------------------------------------------------------------------
// CheckableLineWidget::Model

class CheckableLineWidget::Model: public QAbstractItemModel
{
    using base_type = QAbstractItemModel;

public:
    using CheckStateChangedNotify = std::function<void(Qt::CheckState)>;

    Model(CheckStateChangedNotify checkStateChanged, QObject* parent = nullptr):
        base_type(parent),
        m_checkStateChanged(checkStateChanged)
    {
        m_data[Qt::CheckStateRole] = qVariantFromValue<int>(Qt::Unchecked);
        m_data[Qt::DisplayRole] = lit("<All Items>"); //< Just a default.
    }

    virtual QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid() || row != 0 || column < 0 || column > ColumnCount)
            return QModelIndex();

        return createIndex(row, column);
    }

    virtual QModelIndex parent(const QModelIndex& /*child*/) const override
    {
        return QModelIndex();
    }

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 1;
    }

    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : ColumnCount;
    }

    virtual Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!isValidIndex(index))
            return Qt::NoItemFlags;

        Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
        if (index.column() == CheckColumn)
            result |= Qt::ItemIsUserCheckable;

        return result;
    }

    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        if (!isValidIndex(index))
            return QVariant();

        switch (role)
        {
            // CheckStateRole relates only to CheckColumn.
            case Qt::CheckStateRole:
                return (index.column() == CheckColumn) ? m_data[role] : QVariant();

            // Text and decoration roles relate only to NameColumn.
            case Qt::DisplayRole:
            case Qt::DecorationRole:
            case Qt::ToolTipRole:
            case Qt::StatusTipRole:
            case Qt::WhatsThisRole:
            case Qt::AccessibleTextRole:
            case Qt::AccessibleDescriptionRole:
                return (index.column() != CheckColumn) ? m_data[role] : QVariant();

            // All other roles are column-independent.
            default:
                return m_data[role];
        }
    }

    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
        // Only check state may be changed via this method.
        if (!isValidIndex(index) || index.column() != CheckColumn || role != Qt::CheckStateRole)
            return false;

        return setData(value, role);
    }

    QVariant data(int role) const
    {
        return m_data[role];
    }

    bool setData(const QVariant& value, int role)
    {
        auto& storedValue = m_data[role];
        if (storedValue == value)
            return false;

        storedValue = value;
        emit dataChanged(this->index(0, 0), this->index(0, ColumnCount - 1));

        if (role == Qt::CheckStateRole && m_checkStateChanged)
            m_checkStateChanged(static_cast<Qt::CheckState>(value.toInt()));

        return true;
    }

private:
    bool isValidIndex(const QModelIndex& index) const
    {
        return index.model() == this && index.row() == 0
            && index.column() >= 0 && index.column() < ColumnCount;
    }

private:
    QHash<int, QVariant> m_data;
    CheckStateChangedNotify m_checkStateChanged;
};

//-------------------------------------------------------------------------------------------------
// CheckableLineWidget::PrivateData

struct CheckableLineWidget::PrivateData
{
    QnTreeView* view = nullptr;
    CheckableLineWidget::Model* model = nullptr;

    explicit PrivateData(CheckableLineWidget* parent):
        view(new QnTreeView(parent)),
        model(new CheckableLineWidget::Model(
            [parent](Qt::CheckState newState) { emit parent->checkStateChanged(newState); },
            view))
    {
        view->setModel(model);
        view->setHeaderHidden(true);
        view->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        view->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        view->setSelectionMode(QAbstractItemView::NoSelection);
        view->setRootIsDecorated(false);
        view->setItemsExpandable(false);
        view->setAllColumnsShowFocus(true);
        view->setFrameStyle(0);

        view->header()->setStretchLastSection(false);
        view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        view->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    }
};

//-------------------------------------------------------------------------------------------------
// CheckableLineWidget

CheckableLineWidget::CheckableLineWidget(QWidget* parent):
    base_type(parent),
    m_data(new PrivateData(this))
{
    auto layout = new QVBoxLayout(this);
    layout->addWidget(m_data->view);
}

CheckableLineWidget::~CheckableLineWidget()
{
}

QnTreeView* CheckableLineWidget::view() const
{
    return m_data->view;
}

QString CheckableLineWidget::text() const
{
    return data(Qt::DisplayRole).toString();
}

void CheckableLineWidget::setText(const QString& value)
{
    setData(value, Qt::DisplayRole);
}

QIcon CheckableLineWidget::icon() const
{
    return data(Qt::DecorationRole).value<QIcon>();
}

void CheckableLineWidget::setIcon(const QIcon& value)
{
    setData(qVariantFromValue(value), Qt::DecorationRole);
}

bool CheckableLineWidget::checked() const
{
    return checkState() == Qt::Checked;
}

void CheckableLineWidget::setChecked(bool value)
{
    setCheckState(value ? Qt::Checked : Qt::Unchecked);
}

Qt::CheckState CheckableLineWidget::checkState() const
{
    return static_cast<Qt::CheckState>(data(Qt::CheckStateRole).toInt());
}

void CheckableLineWidget::setCheckState(Qt::CheckState value)
{
    setData(qVariantFromValue<int>(value), Qt::CheckStateRole);
}

QVariant CheckableLineWidget::data(int role) const
{
    return m_data->model->data(role);
}

bool CheckableLineWidget::setData(const QVariant& value, int role)
{
    return m_data->model->setData(value, role);
}

QSize CheckableLineWidget::sizeHint() const
{
    const QSize contentSize(
        style::Metrics::kMinimumButtonWidth, //< Sensible minimum.
        m_data->view->sizeHintForRow(0));

    const auto margins = contentsMargins();
    return contentSize + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize CheckableLineWidget::minimumSizeHint() const
{
    return sizeHint();
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
