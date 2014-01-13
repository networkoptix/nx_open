#ifndef PTZ_TOUR_WIDGET_H
#define PTZ_TOUR_WIDGET_H

#include <QWidget>

#include <core/ptz/ptz_fwd.h>

class QnPtzTourModel;

namespace Ui {
    class PtzTourWidget;
}

class QnPtzTourWidget : public QWidget {
    Q_OBJECT

public:
    explicit QnPtzTourWidget(QWidget *parent = 0);
    ~QnPtzTourWidget();

    // TODO: #GDM where are getters? Symmetry breaking.

    void setPtzTour(const QnPtzTour &tour);
    void setPtzPresets(const QnPtzPresetList &presets);

    // TODO: #GDM signal is never emitted. Is it needed?
signals:
    void tourChanged(const QnPtzTour &tour); 

private slots:
    void at_addSpotButton_clicked();
    void at_deleteSpotButton_clicked();
    void at_moveSpotUpButton_clicked();
    void at_moveSpotDownButton_clicked();

private:
    QScopedPointer<Ui::PtzTourWidget> ui;
    QnPtzTourModel *m_model;
};

#endif // PTZ_TOUR_WIDGET_H
