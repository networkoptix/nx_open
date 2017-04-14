#include "io_module_form_overlay_contents.h"
#include "io_module_overlay_contents_p.h"


class QnIoModuleFormOverlayContentsPrivate: public QnIoModuleOverlayContentsPrivate
{
    Q_OBJECT
    using base_type = QnIoModuleOverlayContentsPrivate;

    Q_DISABLE_COPY(QnIoModuleFormOverlayContentsPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleFormOverlayContents)

    QnIoModuleFormOverlayContents* const q_ptr;

public:
    QnIoModuleFormOverlayContentsPrivate(QnIoModuleFormOverlayContents* main);

protected:
    virtual PortItem* createItem(const QString& id, bool isOutput);

private:
    class InputPortItem;
    class OutputPortItem;
    class Layout;
};
