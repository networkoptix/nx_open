// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_search_widget.h"
#include "private/abstract_search_widget_p.h"

#include <QtWidgets/QMenu>

#include <nx/vms/client/desktop/style/helper.h>

namespace nx::vms::client::desktop {

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

CommonObjectSearchSetup* AbstractSearchWidget::commonSetup() const
{
    return d->commonSetup();
}

void AbstractSearchWidget::resetFilters()
{
    d->resetFilters();
}

void AbstractSearchWidget::setPreviewToggled(bool value)
{
    d->setPreviewToggled(value);
}

bool AbstractSearchWidget::previewToggled() const
{
    return d->previewToggled();
}

void AbstractSearchWidget::setFooterToggled(bool value)
{
    d->setFooterToggled(value);
}

bool AbstractSearchWidget::footerToggled() const
{
    return d->footerToggled();
}

void AbstractSearchWidget::setPlaceholderPixmap(const QPixmap& value)
{
    d->setPlaceholderPixmap(value);
}

SelectableTextButton* AbstractSearchWidget::createCustomFilterButton()
{
    return d->createCustomFilterButton();
}

void AbstractSearchWidget::addFilterWidget(QWidget* widget, Qt::Alignment alignment)
{
    d->addFilterWidget(widget, alignment);
}

QMenu* AbstractSearchWidget::createDropdownMenu()
{
    auto result = new QMenu(this);
    result->setProperty(style::Properties::kMenuAsDropdown, true);
    fixMenuFlags(result);
    return result;
}

void AbstractSearchWidget::fixMenuFlags(QMenu* menu)
{
    menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);
}

void AbstractSearchWidget::addDeviceDependentAction(
    QAction* action, const QString& mixedString, const QString& cameraString)
{
    d->addDeviceDependentAction(action, mixedString, cameraString);
}

EventRibbon* AbstractSearchWidget::view() const
{
    return d->view();
}

bool AbstractSearchWidget::isAllowed() const
{
    return d->isAllowed();
}

void AbstractSearchWidget::updateAllowance()
{
    d->setAllowed(calculateAllowance());
}

void AbstractSearchWidget::addSearchAction(QAction* action)
{
    d->addSearchAction(action);
}

QString AbstractSearchWidget::makePlaceholderText(const QString& title, const QString& description)
{
    static const QString kTemplate = "<center><p>%1</p><p><font size='-1'>%2</font></p></center>";
    return kTemplate.arg(title, description);
}

} // namespace nx::vms::client::desktop
