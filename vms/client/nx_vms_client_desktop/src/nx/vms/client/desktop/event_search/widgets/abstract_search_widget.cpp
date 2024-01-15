// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_search_widget.h"

#include <QtWidgets/QMenu>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/html/html.h>

#include "private/abstract_search_widget_p.h"

namespace nx::vms::client::desktop {

namespace {

static bool isCamera(const QnSharedResourcePointer<QnResource>& resource)
{
    return (bool) resource.objectCast<QnVirtualCameraResource>();
};

} // namespace

AbstractSearchWidget::AbstractSearchWidget(
    WindowContext* context,
    AbstractSearchListModel* model,
    QWidget* parent)
    :
    base_type(parent),
    WindowContextAware(context),
    d(new Private(this, model))
{
    setRelevantControls(Control::defaults);

    auto listenPermissionsChanges =
        [this]()
        {
            connect(system()->accessController(), &core::AccessController::permissionsMaybeChanged,
                this,
                [this](const QnResourceList& resources)
                {
                    if (resources.empty()
                        || std::any_of(resources.cbegin(), resources.cend(), isCamera))
                    {
                        updateAllowance();
                    }
                });
        };

    connect(context, &WindowContext::beforeSystemChanged, this,
        [this]() { system()->accessController()->disconnect(this); } );
    connect(context, &WindowContext::systemChanged, this, listenPermissionsChanges);

    listenPermissionsChanges();

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

void AbstractSearchWidget::setSearchDelay(std::chrono::milliseconds delay)
{
    d->setSearchDelay(delay);
}

QString AbstractSearchWidget::makePlaceholderText(const QString& title, const QString& description)
{
    using namespace nx::vms::common::html;

    QStringList lines = {paragraph(title)};

    if (!description.isEmpty())
        lines.push_back(paragraph(NX_FMT("<font size='-1'>%1</font>", description)));

    return tagged(lines.join(""), "center");
}

} // namespace nx::vms::client::desktop
