// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tableview_columns_calculator.h"

#include <QtGui/QFontMetricsF>
#include <QtQuick/private/qquicktableview_p.h>
#include <QtQuickTemplates2/private/qquickcontrol_p.h>
#include <QtQuickTemplates2/private/qquickheaderview_p.h>

#include <client/client_globals.h>
#include <nx/vms/client/desktop/common/models/sort_filter_proxy_model.h>
#include <utils/common/delayed.h>

namespace {
static constexpr qreal kMinimumColumnWidth = 30.0;
} // namespace

namespace nx::vms::client::desktop {

struct TableViewColumnsCalculator::Private
{
    TableViewColumnsCalculator* const q;
    QQuickTableView* tableView = nullptr;
    SortFilterProxyModel* model = nullptr;
    qreal allColumnWidth = 0;
    QMap<int, qreal> initialWidths;
    QMap<int, qreal> widths;
    QQuickItem* headerContentItem = nullptr;

    void updateColumnWidths()
    {
        if (tableView->columns() == 0)
            return;

        initialWidths.clear();
        allColumnWidth = 0;

        auto delegates = headerContentItem->childItems();
        const QQuickControl* headerButton = nullptr;
        QFont font;
        if (!delegates.isEmpty())
        {
            headerButton = delegates[0]->findChild<QQuickControl*>("headerButton");
            font = headerButton->property("font").value<QFont>();
        }
        QFontMetricsF fm(font);

        for (int col = 0; col < model->columnCount({}); ++col)
        {
            qreal width = delegates.isEmpty()
                ? 0
                : fm.horizontalAdvance(model->headerData(col, Qt::Horizontal).toString()) +
                    headerButton->leftPadding() +
                    headerButton->rightPadding() +
                    16;
            for (int row = 0; row < model->rowCount({}); ++row)
            {
                QModelIndex index = model->index(row, col);
                QVariantMap modelData = {
                    {"display", index.data(Qt::DisplayRole)},
                    {"iconKey", index.data(core::ResourceIconKeyRole)},
                    {"colors", index.data(Qt::ForegroundRole)},
                    };
                QVariantMap properties = {
                    {"cellHeight", tableView->property("cellHeight")},
                    {"tooltipData", {}},
                    {"modelData", modelData},
                    {"isCurrentRow", false},
                    {"isHoveredRow", false},
                    };
                auto delegateItem = tableView->delegate()->createWithInitialProperties(properties);
                width = qMax(width, delegateItem->property("implicitWidth").toReal());
                delete delegateItem;
            }
            initialWidths[col] = width;
            allColumnWidth += width;
        }
        normalize();
        executeLater([this]() { tableView->forceLayout(); }, q);
    }

    void normalize()
    {
        qreal availableWidth = tableView->width() -
            tableView->leftMargin() -
            tableView->rightMargin() -
            tableView->columnSpacing() * (tableView->columns() - 1);
        widths.clear();
        for (int col = 0; col < initialWidths.count(); ++col)
        {
            if (allColumnWidth == 0)
                widths[col] = kMinimumColumnWidth;
            else
                widths[col] = initialWidths[col] / allColumnWidth * availableWidth;
        }
    }
};

TableViewColumnsCalculator::TableViewColumnsCalculator(QObject* parent):
    base_type(parent),
    d(new Private{.q = this})
{
}

TableViewColumnsCalculator::~TableViewColumnsCalculator()
{
}

qreal TableViewColumnsCalculator::columnWidth(int index) const
{
    return qMax(d->widths.value(index, -1), kMinimumColumnWidth);
}

void TableViewColumnsCalculator::componentComplete()
{
    if (auto tableView = qobject_cast<QQuickTableView*>(parent()))
    {
        d->tableView = tableView;
        if (auto model = d->tableView->property("model").value<SortFilterProxyModel*>())
            d->model = model;

        connect(d->tableView, &QQuickItem::widthChanged, this, [this](){ d->normalize(); });

        connect(d->tableView, &QQuickTableView::rowsChanged, this,
            [this](){ d->updateColumnWidths(); });

        connect(d->tableView, &QQuickTableView::columnsChanged, this,
            [this](){ d->updateColumnWidths(); });

        auto headerView = tableView->findChild<QQuickHorizontalHeaderView*>("headerView");
        d->headerContentItem = headerView->contentItem();
    }
}

void TableViewColumnsCalculator::registerQmlType()
{
    qmlRegisterType<TableViewColumnsCalculator>("Nx.Core", 1, 0, "TableViewColumnsCalculator");
}

} // namespace nx::vms::client::desktop {
