#include "abstract_search_widget.h"
#include "private/abstract_search_widget_p.h"

#include <QtWidgets/QMenu>

#include <ui/style/helper.h>

namespace nx::client::desktop {

AbstractSearchWidget::AbstractSearchWidget(
    QnWorkbenchContext* context,
    AbstractSearchListModel* model,
    QWidget* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(context),
    d(new Private(this, model))
{
    setRelevantControls(Control::defaults);
}

AbstractSearchWidget::~AbstractSearchWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

AbstractSearchListModel* AbstractSearchWidget::model() const
{
    return d->model();
}

void AbstractSearchWidget::goToLive()
{
    d->goToLive();
}

AbstractSearchWidget::Controls AbstractSearchWidget::relevantControls() const
{
    return d->relevantControls();
}

void AbstractSearchWidget::setRelevantControls(Controls value)
{
    return d->setRelevantControls(value);
}

AbstractSearchWidget::Period AbstractSearchWidget::selectedPeriod() const
{
    return d->selectedPeriod();
}

QnTimePeriod AbstractSearchWidget::timePeriod() const
{
    return d->currentTimePeriod();
}

AbstractSearchWidget::Cameras AbstractSearchWidget::selectedCameras() const
{
    return d->selectedCameras();
}

QnVirtualCameraResourceSet AbstractSearchWidget::cameras() const
{
    return d->cameras();
}

QString AbstractSearchWidget::textFilter() const
{
    return d->textFilter();
}

QRectF AbstractSearchWidget::selectedArea() const
{
    return d->selectedArea();
}

void AbstractSearchWidget::resetFilters()
{
    d->resetFilters();
}

void AbstractSearchWidget::setPlaceholderPixmap(const QPixmap& value)
{
    d->setPlaceholderPixmap(value);
}

SelectableTextButton* AbstractSearchWidget::createCustomFilterButton()
{
    return d->createCustomFilterButton();
}

QMenu* AbstractSearchWidget::createDropdownMenu()
{
    auto result = new QMenu(this);
    result->setProperty(style::Properties::kMenuAsDropdown, true);
    result->setWindowFlags(result->windowFlags() | Qt::BypassGraphicsProxyWidget);
    return result;
}

void AbstractSearchWidget::addDeviceDependentAction(
    QAction* action, const QString& mixedString, const QString& cameraString)
{
    d->addDeviceDependentAction(action, mixedString, cameraString);
}

} // namespace nx::client::desktop
