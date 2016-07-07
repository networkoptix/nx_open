#ifndef QN_SELECTION_OVERLAY_HACK_INSTRUMENT_H
#define QN_SELECTION_OVERLAY_HACK_INSTRUMENT_H



#include "instrument.h"

class QnResourceWidget;

/**
 * This instrument implements some corrections to when selection overlay 
 * (the blue tint that you get for selected items) is displayed.
 * 
 * Generally, selection overlay is not displayed when there is only one item 
 * selected, but there are exceptions.
 */
class SelectionOverlayHackInstrument: public Instrument {
    Q_OBJECT;
    typedef Instrument base_type;

public:
    SelectionOverlayHackInstrument(QObject *parent = NULL);

    virtual ~SelectionOverlayHackInstrument();

protected slots:
    void at_scene_selectionChanged();

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    QnResourceWidget *singleSelectedWidget();

    void showSelectedOverlay(QnResourceWidget *widget, bool show);

private:
    QPointer<QnResourceWidget> m_singleSelectedWidget;
};


#endif // QN_SELECTION_OVERLAY_HACK_INSTRUMENT_H
