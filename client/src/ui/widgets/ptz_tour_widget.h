#ifndef PTZ_TOUR_WIDGET_H
#define PTZ_TOUR_WIDGET_H

#include <QWidget>

#include <core/ptz/ptz_fwd.h>

class QnPtzTourModel;

namespace Ui {
class QnPtzTourWidget;
}

class QnPtzTourWidget : public QWidget {
    Q_OBJECT

public:
    explicit QnPtzTourWidget(QWidget *parent = 0);
    ~QnPtzTourWidget();

    void setPtzTour(const QnPtzTour &tour);
    void setPtzPresets(const QnPtzPresetList &presets);

signals:
    void tourChanged(const QnPtzTour &tour);
private:
    QScopedPointer<Ui::QnPtzTourWidget> ui;
    QnPtzTourModel *m_model;
};

#endif // PTZ_TOUR_WIDGET_H
