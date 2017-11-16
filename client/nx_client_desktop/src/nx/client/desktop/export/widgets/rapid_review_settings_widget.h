#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class RapidReviewSettingsWidget; }

namespace nx {
namespace client {
namespace desktop {

class RapidReviewSettingsWidgetPrivate;
class RapidReviewSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    RapidReviewSettingsWidget(QWidget* parent = nullptr);

    virtual ~RapidReviewSettingsWidget();

    qint64 sourcePeriodLengthMs() const;
    void setSourcePeriodLengthMs(qint64 lengthMs);
    static qint64 minimalSourcePeriodLengthMs();

    void setSpeed(int value);
    int speed() const;

    qint64 frameStepMs() const; //< 0 means source frame step.

    /* Magic knowledge. We know that the result will be generated with 30 fps. --rvasilenko */
    static const int kResultFps = 30;

signals:
    void speedChanged(int absoluteSpeed, qint64 frameStepMs);
    void deleteClicked();

private:
    void updateRanges();
    void updateControls();

private:
    const QScopedPointer<Ui::RapidReviewSettingsWidget> ui;
    const QScopedPointer<RapidReviewSettingsWidgetPrivate> d;
    bool m_updating = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
