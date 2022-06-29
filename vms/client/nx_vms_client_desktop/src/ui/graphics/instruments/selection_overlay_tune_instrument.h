// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include "instrument.h"

class QnResourceWidget;

/**
 * This instrument implements some corrections to when selection overlay
 * (the blue tint that you get for selected items) is displayed.
 *
 * Generally, selection overlay is not displayed when there is only one item
 * selected, but there are exceptions.
 */
class SelectionOverlayTuneInstrument: public Instrument {
    Q_OBJECT;
    typedef Instrument base_type;

public:
    SelectionOverlayTuneInstrument(QObject *parent = nullptr);

    virtual ~SelectionOverlayTuneInstrument();

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
