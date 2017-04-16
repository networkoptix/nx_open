#include "io_module_form_overlay_contents.h"
#include "io_module_form_overlay_contents_p.h"


QnIoModuleFormOverlayContents::QnIoModuleFormOverlayContents() :
    base_type(),
    d_ptr(new QnIoModuleFormOverlayContentsPrivate(this))
{
}

QnIoModuleFormOverlayContents::~QnIoModuleFormOverlayContents()
{
}

void QnIoModuleFormOverlayContents::portsChanged(const QnIOPortDataList& ports, bool userInputEnabled)
{
    Q_D(QnIoModuleFormOverlayContents);
    d->portsChanged(ports, userInputEnabled);
}

void QnIoModuleFormOverlayContents::stateChanged(const QnIOPortData& port, const QnIOStateData& state)
{
    Q_D(QnIoModuleFormOverlayContents);
    d->stateChanged(port, state);
}
