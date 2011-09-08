#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include "unmoved/unmoved_interactive_opacity_item.h"

class QGraphicsProxyWidget;
class QGraphicsWidget;

class QTimerEvent;
class CLVideoCamera;
class TimeSlider;
class SpeedWidget;
class VolumeSlider;
class MyTextItem;

class MyLable : public QLabel
{
protected:
    void wheelEvent(QWheelEvent *) {} // to avoid scene move up and down
};

class MyButton : public QAbstractButton
{
public:
    MyButton(QWidget *parent = 0) : QAbstractButton(parent) {}

    void setPixmap(const QPixmap &p) { m_pixmap = p; }
    void setPressedPixmap(const QPixmap &p) { m_pressedPixmap = p; }
    void setCheckedPixmap(const QPixmap &p) { m_checkedPixmap = p; }
//    QSize sizeHint() const {return m_pixmap.size(); }

protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        if (!isDown())
            p.drawPixmap(contentsRect(), m_pixmap);

        else if (isEnabled())
            p.drawPixmap(contentsRect(), m_pressedPixmap);

        if (isChecked())
            p.drawPixmap(contentsRect(), m_checkedPixmap);
    }

    void wheelEvent(QWheelEvent *) {} // to avoid scene move up and down

private:
    QPixmap m_pixmap;
    QPixmap m_pressedPixmap;
    QPixmap m_checkedPixmap;
};

class NavigationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationWidget(QWidget *parent = 0);

    TimeSlider *slider() const;
    QLabel *label() const;

Q_SIGNALS:
    void rewindBackward();
    void rewindForward();
    void stepBackward();
    void stepForward();

protected:
    void wheelEvent(QWheelEvent *) {} // to avoid scene move up and down

private:
    QHBoxLayout *m_layout;
    TimeSlider *m_slider;
    MyLable *m_label;

    friend class NavigationItem;
};

class ImageButtonItem;
class NavigationItem : public CLUnMovedInteractiveOpacityItem
{
    Q_OBJECT
public:
    explicit NavigationItem(QGraphicsItem *parent = 0);
    ~NavigationItem();

    QWidget *navigationWidget();
    QGraphicsWidget *graphicsWidget() const { return m_graphicsWidget; }

    void setVideoCamera(CLVideoCamera* camera);

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {}
    QRectF boundingRect() const;

    bool mouseOver() const { return m_mouseOver; }

    bool isPlaying() const { return m_playing; }
    void setPlaying(bool playing);

    bool isActive() const;
    void setActive(bool active);

    static const int DEFAULT_HEIGHT = 60;

protected:
    void timerEvent(QTimerEvent* event);
    void updateSlider();


private slots:
    void onValueChanged(qint64);
    void pause();
    void play();
    void togglePlayPause();

    void rewindBackward();
    void rewindForward();

    void stepBackward();
    void stepForward();

    void onSliderPressed();
    void onSliderReleased();

    void onSpeedChanged(float);

    void onVolumeLevelChanged(int);
    void restoreInfoText();

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *);
    void wheelEvent(QGraphicsSceneWheelEvent *);

private:
    QGraphicsProxyWidget *m_proxy;
    NavigationWidget *m_widget;
    ImageButtonItem *m_stepBackwardButton;
    ImageButtonItem *m_backwardButton;
    ImageButtonItem *m_playButton;
    ImageButtonItem *m_forwardButton;
    ImageButtonItem *m_stepForwardButton;
    SpeedWidget *m_speedWidget;
    QGraphicsWidget *m_graphicsWidget;
    ImageButtonItem *m_muteButton;
    VolumeSlider *m_volumeSlider;

    CLVideoCamera* m_camera;
    int m_timerId;
    bool m_sliderIsmoving;
    qint64 m_currentTime;
    bool m_mouseOver;
    bool m_playing;

    MyTextItem *m_textItem;
    bool m_active;

    struct RestoreInfoTextData {
        QString extraInfoText;
        QTimer timer;
    } *restoreInfoTextData;
};

#endif // NAVIGATIONITEM_H
