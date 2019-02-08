#pragma once

#include "io_module_overlay_widget.h"

/*
* Tile-styled I/O module overlay contents widget.
*
* Uses the following palette entries:
*  WindowText: inactive port id label
*  HighlightedText: active port id label
*  ButtonText: port name label
*  Button: output port button background
*  Dark: output port button background (pressed)
*  Midlight: output port button background (hovered)
*  Shadow: pressed output port button shadows
*/
class QnIoModuleGridOverlayContentsPrivate;
class QnIoModuleGridOverlayContents: public QnIoModuleOverlayContents
{
    Q_OBJECT
    using base_type = QnIoModuleOverlayContents;

public:
    QnIoModuleGridOverlayContents();
    virtual ~QnIoModuleGridOverlayContents();

protected:
    virtual void portsChanged(const QnIOPortDataList& ports, bool userInputEnabled) override;
    virtual void stateChanged(const QnIOPortData& port, const QnIOStateData& state) override;

private:
    QScopedPointer<QnIoModuleGridOverlayContentsPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnIoModuleGridOverlayContents)
};
