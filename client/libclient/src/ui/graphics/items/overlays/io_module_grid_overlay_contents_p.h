#include "io_module_grid_overlay_contents.h"
#include "io_module_overlay_contents_p.h"


class QnIoModuleGridOverlayContentsPrivate: public QnIoModuleOverlayContentsPrivate
{
    Q_OBJECT
    using base_type = QnIoModuleOverlayContentsPrivate;

    Q_DISABLE_COPY(QnIoModuleGridOverlayContentsPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleGridOverlayContents)

    QnIoModuleGridOverlayContents* const q_ptr;

public:
    QnIoModuleGridOverlayContentsPrivate(QnIoModuleGridOverlayContents* main);

protected:
    virtual PortItem* createItem(const QString& id, bool isOutput);

private:
    class InputPortItem;
    class OutputPortItem;
    class Layout;
};
