#include "item_view_auto_hider.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QStackedWidget>

#include <ui/style/helper.h>
#include <nx/vms/client/desktop/common/widgets/table_view.h>

#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMessageFontPixelSize = 16;
static constexpr int kMessageFontWeight = QFont::Normal;

} // namespace

// ------------------------------------------------------------------------------------------------
// ItemViewAutoHider::Private

class ItemViewAutoHider::Private: public QObject
{
    using base_type = QObject;

public:
    Private(ItemViewAutoHider* q):
        base_type(),
        label(new QLabel()),
        stackedWidget(new QStackedWidget()),
        viewPage(new QWidget()),
        labelPage(new QWidget())
    {
        QFont font;
        label->setForegroundRole(QPalette::WindowText);
        label->setProperty(style::Properties::kDontPolishFontProperty, true);
        font.setPixelSize(kMessageFontPixelSize);
        font.setWeight(kMessageFontWeight);
        label->setFont(font);
        label->setAlignment(Qt::AlignCenter);

        auto labelLayout = new QVBoxLayout(labelPage);
        labelLayout->setContentsMargins(0, 0, 0, 0);
        labelLayout->addWidget(label);

        auto viewLayout = new QVBoxLayout(viewPage);
        viewLayout->setContentsMargins(0, 0, 0, 0);

        stackedWidget->addWidget(viewPage);
        stackedWidget->addWidget(labelPage);

        auto mainLayout = new QVBoxLayout(q);
        mainLayout->addWidget(stackedWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
    }

    bool isViewHidden() const
    {
        return stackedWidget->currentWidget() != viewPage;
    }

    void setViewHidden(bool hidden)
    {
        stackedWidget->setCurrentWidget(hidden ? labelPage : viewPage);
    }

    QAbstractItemView* setView(QAbstractItemView* newView)
    {
        QAbstractItemView* orphanView = nullptr;
        if (view != newView)
        {
            orphanView = view;
            view = newView;

            if (orphanView)
            {
                viewPage->layout()->removeWidget(orphanView);
                orphanView->setParent(nullptr);
            }

            if (newView)
                viewPage->layout()->addWidget(newView);
        }

        setModel(view ? view->model() : nullptr);
        updateState();

        return orphanView;
    }

    void setModel(QAbstractItemModel* newModel)
    {
        if (model == newModel)
            return;

        if (model)
            model->disconnect(this);

        model = newModel;

        if (newModel)
        {
            connect(model, &QAbstractItemModel::modelReset, this, &Private::updateState);
            connect(model, &QAbstractItemModel::rowsInserted, this, &Private::updateState);
            connect(model, &QAbstractItemModel::rowsRemoved, this, &Private::updateState);
        }
    }

    void updateState()
    {
        bool emptyView = !view || !model || model->rowCount(view->rootIndex()) == 0;
        setViewHidden(emptyView);
    }

public:
    QPointer<QAbstractItemView> view;
    QPointer<QAbstractItemModel> model;
    QLabel* const label;

    QStackedWidget* const stackedWidget;
    QWidget* const viewPage;
    QWidget* const labelPage;
};

// ------------------------------------------------------------------------------------------------
// ItemViewAutoHider

ItemViewAutoHider::ItemViewAutoHider(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    connect(d->stackedWidget, &QStackedWidget::currentChanged, this,
        [this]() { emit viewVisibilityChanged(isViewHidden()); });
}

ItemViewAutoHider::~ItemViewAutoHider()
{
}

QAbstractItemView* ItemViewAutoHider::view() const
{
    return d->view;
}

QAbstractItemView* ItemViewAutoHider::setView(QAbstractItemView* view)
{
    return d->setView(view);
}

QLabel* ItemViewAutoHider::emptyViewMessageLabel() const
{
    return d->label;
}

QString ItemViewAutoHider::emptyViewMessage() const
{
    return d->label->text();
}

void ItemViewAutoHider::setEmptyViewMessage(const QString& message)
{
    d->label->setText(message);
}

bool ItemViewAutoHider::isViewHidden() const
{
    return d->isViewHidden();
}

ItemViewAutoHider* ItemViewAutoHider::create(QAbstractItemView* view, const QString& message)
{
    auto parent = view ? view->parentWidget() : nullptr;
    auto autoHider = new ItemViewAutoHider(parent);

    if (parent && parent->layout())
        parent->layout()->replaceWidget(view, autoHider);

    autoHider->setEmptyViewMessage(message);
    autoHider->setView(view);

    return autoHider;
}

} // namespace nx::vms::client::desktop
