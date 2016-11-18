#include "item_view_auto_hider.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QStackedWidget>

#include <ui/style/helper.h>
#include <ui/widgets/common/table_view.h>

#include <utils/common/event_processors.h>

namespace {

static const int kMessageFontPixelSize = 16;
static const int kMessageFontWeight = QFont::Normal;

} // namespace

/*
* QnItemViewAutoHiderPrivate
*/
class QnItemViewAutoHiderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(QnItemViewAutoHider);
    QnItemViewAutoHider* q_ptr;

public:
    QnItemViewAutoHiderPrivate(QnItemViewAutoHider* q):
        QObject(),
        q_ptr(q),
        view(nullptr),
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

        auto labelLayout = new QVBoxLayout(viewPage);
        labelLayout->setContentsMargins(0, 0, 0, 0);
        labelLayout->addWidget(label);

        auto viewLayout = new QVBoxLayout(viewPage);
        viewLayout->setContentsMargins(0, 0, 0, 0);

        stackedWidget->addWidget(viewPage);
        stackedWidget->addWidget(labelPage);

        auto mainLayout = new QVBoxLayout(q);
        mainLayout->addWidget(stackedWidget);
    }

    void setView(QAbstractItemView* newView)
    {
        if (view == newView)
            return;

        delete view;
        view = newView;

        viewPage->layout()->addWidget(view);
        view->installEventFilter(this);
    }

    void updateState()
    {
        bool emptyView = !view || !view->model() || view->model()->rowCount() == 0;
        stackedWidget->setCurrentWidget(emptyView ? labelPage : viewPage);
    }

    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        NX_ASSERT(watched == view);
        if (event->type() != QEvent::Paint)
            return false;

        updateState();
        return false;
    }

public:
    QAbstractItemView* view;
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

void QnItemViewAutoHider::setView(QAbstractItemView* view)
{
    Q_D(QnItemViewAutoHider);
    d->setView(view);
}

QLabel* QnItemViewAutoHider::emptyMessageLabel() const
{
    Q_D(const QnItemViewAutoHider);
    return d->label;
}

QString QnItemViewAutoHider::emptyMessage() const
{
    Q_D(const QnItemViewAutoHider);
    return d->label->text();
}

void QnItemViewAutoHider::setEmptyMessage(const QString& message)
{
    Q_D(QnItemViewAutoHider);
    d->label->setText(message);
}
