#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

#include "unmoved/unmoved_interactive_opacity_item.h"
#include "volumewidget.h"

class QTimerEvent;
class CLVideoCamera;
class TimeSlider;
class QGraphicsProxyWidget;
class VolumeWidget;
class MyTextItem;
class MyButton : public QAbstractButton
{
public:
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

    bool isPlaying() const { return m_playing; }
    void setPlaying(bool);

private slots:
    void togglePlayPause();

signals:
    void pause();
    void play();
    void rewindBackward();
    void rewindForward();
    void stepBackward();
    void stepForward();

protected:
    void wheelEvent(QWheelEvent *) {} // to fix buuug with scene

private:
    QHBoxLayout *m_layout;
    MyButton *m_backwardButton;
    MyButton *m_playButton;
    MyButton *m_stepForwardButton;
    MyButton *m_stepBackwardButton;
    MyButton *m_forwardButton;
    bool m_playing;
    QPushButton *m_pauseButton;
    TimeSlider *m_slider;
    QLabel *m_label;
    VolumeWidget *m_volumeWidget;

    friend class NavigationItem;
};

class NavigationItem : public CLUnMovedInteractiveOpacityItem
{
    Q_OBJECT
public:
    explicit NavigationItem(QGraphicsItem *parent = 0);
    ~NavigationItem();

    QWidget *navigationWidget();

    void setVideoCamera(CLVideoCamera* camera);

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {}
    QRectF boundingRect() const;

    bool mouseOver() const { return m_mouseOver; }

    static const int DEFAULT_HEIGHT = 60;

protected:
    void timerEvent(QTimerEvent* event);
    void updateSlider();

private slots:
    void onValueChanged(qint64);
    void pause();
    void play();

    void rewindBackward();
    void rewindForward();

    void stepBackward();
    void stepForward();

    void onSliderPressed();
    void onSliderReleased();
protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *);

private:
    QGraphicsProxyWidget *m_proxy;
    NavigationWidget *m_widget;
    CLVideoCamera* m_camera;
    int m_timerId;
    bool m_sliderIsmoving;
    qint64 m_currentTime;
    bool m_mouseOver;

    MyTextItem *textItem;
};

#endif // NAVIGATIONITEM_H
