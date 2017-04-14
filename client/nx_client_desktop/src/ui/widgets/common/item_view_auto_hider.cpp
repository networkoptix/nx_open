#include "item_view_auto_hider.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QStackedWidget>

#include <ui/style/helper.h>
#include <ui/widgets/common/table_view.h>

#include <utils/common/connective.h>
#include <utils/common/event_processors.h>

namespace {

static const int kMessageFontPixelSize = 16;
static const int kMessageFontWeight = QFont::Normal;

} // namespace

/*
* QnItemViewAutoHiderPrivate
*/
class QnItemViewAutoHiderPrivate: public Connective<QObject>
{
    using base_type = Connective<QObject>;

    Q_DECLARE_PUBLIC(QnItemViewAutoHider);
    QnItemViewAutoHider* q_ptr;

public:
    QnItemViewAutoHiderPrivate(QnItemViewAutoHider* q):
        base_type(),
        q_ptr(q),
        view(nullptr),
        model(nullptr),
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

        connect(stackedWidget, &QStackedWidget::currentChanged, this,
            [this]()
            {
                Q_Q(QnItemViewAutoHider);
                emit q->viewVisibilityChanged(isViewHidden());
            });
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
            connect(model, &QAbstractItemModel::modelReset,
                this, &QnItemViewAutoHiderPrivate::updateState);
            connect(model, &QAbstractItemModel::rowsInserted,
                this, &QnItemViewAutoHiderPrivate::updateState);
            connect(model, &QAbstractItemModel::rowsRemoved,
                this, &QnItemViewAutoHiderPrivate::updateState);
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
    QLabel* label;

    QStackedWidget* stackedWidget;
    QWidget* viewPage;
    QWidget* labelPage;
};


/*
* QnItemViewAutoHider
*/

QnItemViewAutoHider::QnItemViewAutoHider(QWidget* parent):
    base_type(parent),
    d_ptr(new QnItemViewAutoHiderPrivate(this))
{
}

QnItemViewAutoHider::~QnItemViewAutoHider()
{
}

QAbstractItemView* QnItemViewAutoHider::view() const
{
    Q_D(const QnItemViewAutoHider);
    return d->view;
}

QAbstractItemView* QnItemViewAutoHider::setView(QAbstractItemView* view)
{
    Q_D(QnItemViewAutoHider);
    return d->setView(view);
}

QLabel* QnItemViewAutoHider::emptyViewMessageLabel() const
{
    Q_D(const QnItemViewAutoHider);
    return d->label;
}

QString QnItemViewAutoHider::emptyViewMessage() const
{
    Q_D(const QnItemViewAutoHider);
    return d->label->text();
}

void QnItemViewAutoHider::setEmptyViewMessage(const QString& message)
{
    Q_D(QnItemViewAutoHider);
    d->label->setText(message);
}

bool QnItemViewAutoHider::isViewHidden() const
{
    Q_D(const QnItemViewAutoHider);
    return d->isViewHidden();
}

QnItemViewAutoHider* QnItemViewAutoHider::create(QAbstractItemView* view, const QString& message)
{
    auto parent = view ? view->parentWidget() : nullptr;
    auto autoHider = new QnItemViewAutoHider(parent);

    if (parent && parent->layout())
        parent->layout()->replaceWidget(view, autoHider);

    autoHider->setEmptyViewMessage(message);
    autoHider->setView(view);

    return autoHider;
}
