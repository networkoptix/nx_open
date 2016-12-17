#pragma once

#include "io_module_overlay_widget.h"

class QnIoModuleFormOverlayContentsPrivate;
class QnIoModuleFormOverlayContents: public QnIoModuleOverlayContents
{
    Q_OBJECT
    using base_type = QnIoModuleOverlayContents;

public:
    QnIoModuleFormOverlayContents();
    virtual ~QnIoModuleFormOverlayContents();

protected:
    virtual void portsChanged(const QnIOPortDataList& ports, bool userInputEnabled) override;
    virtual void stateChanged(const QnIOPortData& port, const QnIOStateData& state) override;

private:
    QScopedPointer<QnIoModuleFormOverlayContentsPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnIoModuleFormOverlayContents)
};
