// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_model_menu.h"

#include <memory>

#include <QtCore/QtGlobal>
#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPointer>
#include <QtGui/QActionEvent>
#include <QtGui/QScreen>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidgetAction>

#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

namespace {

class MenuItemDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    explicit MenuItemDelegate(QObject* parent = nullptr): base_type(parent)
    {
    }

    virtual void initStyleOption(
        QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        base_type::initStyleOption(option, index);
        if (!option->state.testFlag(QStyle::State_MouseOver))
            option->state.setFlag(QStyle::State_Selected, false);
    }
};

class MenuTreeView: public QTreeView
{
public:
    MenuTreeView(QWidget* parent = nullptr): QTreeView(parent)
    {
        setRootIsDecorated(false);
        setItemsExpandable(false);
        setUniformRowHeights(true);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setHeaderHidden(true);
        setItemDelegate(new MenuItemDelegate(this));
        header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        header()->setStretchLastSection(false);
    }

    virtual QSize minimumSizeHint() const override { return {}; }

protected:
    virtual void rowsInserted(const QModelIndex& parent, int start, int end) override
    {
        QTreeView::rowsInserted(parent, start, end);
        expandAll();
    }

    virtual void reset() override
    {
        QTreeView::reset();
        expandAll();
    }
};

} // namespace

struct ItemModelMenu::Private
{
    ItemModelMenu* const q;
    QPointer<QAbstractItemView> view;
    QPointer<QAbstractItemModel> model;
    std::unique_ptr<QWidgetAction> widgetAction{new QWidgetAction{nullptr}};
    nx::utils::ScopedConnections viewConnections;

    void handleItemEntered(const QModelIndex& index)
    {
        if (!NX_ASSERT(view))
            return;

        view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        emit q->itemHovered(index);
    }

    void handleItemActivated(const QModelIndex& index)
    {
        widgetAction->trigger();
        emit q->itemTriggered(index);
    }

    void handleItemClicked(const QModelIndex& index)
    {
        handleItemActivated(index);
    }

    void updateActionGeometry()
    {
        QActionEvent actionEvent(QEvent::ActionChanged, widgetAction.get());
        QCoreApplication::sendEvent(q, &actionEvent);
    }
};

ItemModelMenu::ItemModelMenu(QWidget* parent):
    base_type(parent),
    d(new Private{this})
{
    setView(new MenuTreeView(this));

    connect(this, &QMenu::aboutToShow, this,
        [this]()
        {
            if (!d->view)
                return;

            auto palette = this->palette();
            palette.setBrush(QPalette::Midlight, palette.highlight());
            palette.setBrush(QPalette::Highlight, Qt::transparent);
            d->view->setPalette(palette);
            d->view->setMaximumSize(screen()->size() / 2);
            d->updateActionGeometry();
        });

    addAction(d->widgetAction.get());
}

ItemModelMenu::~ItemModelMenu()
{
    // Required here for forward-declared scoped pointer destruction.
}

QAbstractItemModel* ItemModelMenu::model() const
{
    return d->model;
}

void ItemModelMenu::setModel(QAbstractItemModel* value)
{
    if (d->model == value)
        return;

    d->model = value;

    if (d->view)
        d->view->setModel(d->model);
}

QAbstractItemView* ItemModelMenu::view() const
{
    return d->view;
}

void ItemModelMenu::setView(QAbstractItemView* value)
{
    if (d->view == value || !d->widgetAction || !NX_ASSERT(value))
        return;

    d->viewConnections.reset();

    d->view = value;

    if (d->view->model() != d->model)
        d->view->setModel(d->model);

    d->view->setAttribute(Qt::WA_MacShowFocusRect, false);
    d->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->view->setMouseTracking(true);
    d->view->setDragEnabled(false);
    d->view->setAutoScroll(false);
    d->view->setTabKeyNavigation(false);
    d->view->setSelectionMode(QAbstractItemView::SingleSelection);
    d->view->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->view->setFrameStyle(QFrame::NoFrame);
    d->view->setLineWidth(0);
    d->view->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    d->view->setEditTriggers(QAbstractItemView::NoEditTriggers);

    d->viewConnections << connect(d->view.data(), &QAbstractItemView::entered, this,
        [this](const QModelIndex& index) { d->handleItemEntered(index); });

    d->viewConnections << connect(d->view.data(), &QAbstractItemView::clicked, this,
        [this](const QModelIndex& index) { d->handleItemClicked(index); });

    d->viewConnections << connect(d->view.data(), &QAbstractItemView::activated, this,
        [this](const QModelIndex& index) { d->handleItemActivated(index); });

    d->widgetAction->setDefaultWidget(d->view);
}

QSize ItemModelMenu::sizeHint() const
{
    // QAbstractScrollArea bug workaround.
    const auto scrollBar = d->view ? d->view->verticalScrollBar() : nullptr;
    const auto scrollBarContainer = scrollBar ? scrollBar->parentWidget() : nullptr;
    if (scrollBarContainer)
        scrollBar->setHidden(scrollBarContainer->isHidden());

    return base_type::sizeHint();
}

bool ItemModelMenu::event(QEvent* event)
{
    if (event->type() == QEvent::LayoutRequest)
    {
        d->updateActionGeometry();
        return true;
    }
    return base_type::event(event);
}

} // namespace nx::vms::client::desktop
