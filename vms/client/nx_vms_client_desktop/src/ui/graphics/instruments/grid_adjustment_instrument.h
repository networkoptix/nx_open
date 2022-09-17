// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_GRID_ADJUSTMENT_INSTRUMENT_H
#define QN_GRID_ADJUSTMENT_INSTRUMENT_H

#include <QtCore/QPointer>

#include "instrument.h"

namespace nx::vms::client::desktop { class Workbench; }

class GridAdjustmentInstrument: public Instrument {
    Q_OBJECT

public:
    GridAdjustmentInstrument(
        nx::vms::client::desktop::Workbench* workbench, QObject* parent = nullptr);

    virtual ~GridAdjustmentInstrument();

    qreal speed() const;

    void setSpeed(qreal speed);

    qreal maxSpacing() const;

    void setMaxSpacing(qreal maxSpacing);

protected:
    virtual bool wheelEvent(QWidget *viewport, QWheelEvent *event) override;
    virtual bool wheelEvent(QGraphicsScene *scene, QGraphicsSceneWheelEvent *event) override;

private:
    nx::vms::client::desktop::Workbench *workbench() const;

private:
    QPointer<QWidget> m_currentViewport;
    QPointer<nx::vms::client::desktop::Workbench> m_workbench;
    qreal m_speed;
    qreal m_maxSpacing;
};


#endif // QN_GRID_ADJUSTMENT_INSTRUMENT_H
