#pragma once

#include "io_module_overlay_widget.h"

/*
* Form-styled I/O module overlay contents widget.
*
* Uses the following palette entries:
*  WindowText: inactive port id label
*  ButtonText: port name label
*  Button: output port button background
*  Dark: output port button background (pressed)
*  Midlight: output port button background (hovered)
*  Shadow: output port button shadows
*/
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
