#include "presented_state_delegate.h"

#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>

#include <ui/style/skin.h>
#include <nx/client/desktop/ui/dialogs/private/found_devices_model.h>

namespace {

using namespace nx::client::desktop::ui;

void removeLayout(QLayout* layout)
{
    if (!layout)
        return;

    while(const auto item = layout->takeAt(0))
    {
        if (const auto widget = item->widget())
            delete widget;
        else if (const auto childLayout = item->layout())
            removeLayout(childLayout);

        delete item;
    }

    delete layout;
}

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
    removeLayout(layout());
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
    currentLayout->addWidget(button);
    currentLayout->addStretch(kBigStretch);

    if (addedState)
    {
        button->setIcon(qnSkin->icon("buttons/checkmark.png"));
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
namespace ui {

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

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
