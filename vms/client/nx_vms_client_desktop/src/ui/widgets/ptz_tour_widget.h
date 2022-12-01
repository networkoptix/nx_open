// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef PTZ_TOUR_WIDGET_H
#define PTZ_TOUR_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/ptz/ptz_fwd.h>

Q_MOC_INCLUDE("core/ptz/ptz_preset.h")
Q_MOC_INCLUDE("core/ptz/ptz_tour.h")

class QnPtzTourSpotsModel;

namespace Ui {
    class PtzTourWidget;
}

class QnPtzTourWidget : public QWidget {
    Q_OBJECT

public:
    explicit QnPtzTourWidget(QWidget *parent = 0);
    ~QnPtzTourWidget();

    const QnPtzTourSpotList &spots() const;
    Q_SLOT void setSpots(const QnPtzTourSpotList &spots);

    const QnPtzPresetList &presets() const;
    Q_SLOT void setPresets(const QnPtzPresetList &presets);

    QnPtzTourSpot currentTourSpot() const;

signals:
    void tourSpotsChanged(const QnPtzTourSpotList &spots);

private slots:
    void at_addSpotButton_clicked();
    void at_deleteSpotButton_clicked();
    void at_moveSpotUpButton_clicked();
    void at_moveSpotDownButton_clicked();

    void at_tableViewport_resizeEvent();
private:
    QScopedPointer<Ui::PtzTourWidget> ui;
    QnPtzTourSpotsModel *m_model;
};

#endif // PTZ_TOUR_WIDGET_H
