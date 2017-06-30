#include "io_module_grid_overlay_contents.h"
#include "io_module_grid_overlay_contents_p.h"


QnIoModuleGridOverlayContents::QnIoModuleGridOverlayContents() :
    base_type(),
    d_ptr(new QnIoModuleGridOverlayContentsPrivate(this))
{
}

QnIoModuleGridOverlayContents::~QnIoModuleGridOverlayContents()
{
}

void QnIoModuleGridOverlayContents::portsChanged(const QnIOPortDataList& ports, bool userInputEnabled)
{
    Q_D(QnIoModuleGridOverlayContents);
    d->portsChanged(ports, userInputEnabled);
}

void QnIoModuleGridOverlayContents::stateChanged(const QnIOPortData& port, const QnIOStateData& state)
{
    Q_D(QnIoModuleGridOverlayContents);
    d->stateChanged(port, state);
}
