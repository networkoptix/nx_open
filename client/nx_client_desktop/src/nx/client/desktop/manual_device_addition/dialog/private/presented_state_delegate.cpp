#include "presented_state_delegate.h"

#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>

#include <ui/style/skin.h>
#include <nx/client/desktop/manual_device_addition/dialog/private/found_devices_model.h>
#include <nx/client/desktop/utils/widget_utils.h>

namespace {

using namespace nx::client::desktop;

class CellWidget: public QWidget
{
    using base_type = QWidget;

public:
    CellWidget(QWidget* parent = nullptr);

    void setState(FoundDevicesModel::PresentedState value);
};

CellWidget::CellWidget(QWidget* parent):
    base_type(parent)
{
}

void CellWidget::setState(FoundDevicesModel::PresentedState value)
{
    WidgetUtils::removeLayout(layout());
    if (value == FoundDevicesModel::notPresentedState)
        return;

    const bool addedState = value == FoundDevicesModel::alreadyAddedState;
    const auto text = addedState
        ? PresentedStateDelegate::tr("Added")
        : PresentedStateDelegate::tr("Adding");

    const auto button = new QPushButton(text, this);
    button->setFlat(true);
    button->setAttribute(Qt::WA_TransparentForMouseEvents);
    button->setFocusPolicy(Qt::NoFocus);

    static constexpr auto kBigStretch = 100;
    const auto currentLayout = new QHBoxLayout(this);
    currentLayout->addStretch(kBigStretch);
    currentLayout->addWidget(button);

    if (addedState)
    {
        button->setIcon(qnSkin->icon("text_buttons/ok.png"));
        return;
    }

    const auto movie = qnSkin->newMovie("legacy/loading.gif", button);
    connect(movie, &QMovie::frameChanged, button,
        [button, movie](int /*frameNumber*/)
        {
            button->setIcon(movie->currentPixmap());
        });

    if (movie->loopCount() != -1)
        connect(movie, &QMovie::finished, movie, &QMovie::start);

    movie->start();
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

PresentedStateDelegate::PresentedStateDelegate(QObject* parent):
    base_type(parent)
{
}

QWidget* PresentedStateDelegate::createEditor(QWidget* parent,
    const QStyleOptionViewItem& /*option*/,
    const QModelIndex& /*index*/) const
{
    return new CellWidget(parent);
}

void PresentedStateDelegate::setEditorData(QWidget *editor,
    const QModelIndex &index) const
{
    if (const auto cell = dynamic_cast<CellWidget*>(editor))
    {
        cell->setState(index.data(FoundDevicesModel::presentedStateRole)
            .value<FoundDevicesModel::PresentedState>());
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
