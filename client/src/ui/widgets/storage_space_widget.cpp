#include "storage_space_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QStyleOption>

#include <ui/style/skin.h>

namespace {
    static const QByteArray stylesheet = "\
        QSlider::groove:horizontal {\
        border: 1px solid #999999;\
        height: 8px; /* the groove expands to the size of the slider by default. by giving it a height, it has a fixed size */\
        background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4);\
        margin: 2px 0;\
    }\
    \
    QSlider::handle:horizontal {\
        background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f);\
        border: 1px solid #5c5c5c;\
        width: 18px;\
        margin: -2px 0; /* handle is placed by default on the contents rect of the groove. Expand outside the groove */\
        border-radius: 3px;\
    }";
}

QnStorageSpaceWidget::QnStorageSpaceWidget(QWidget *parent) :
    base_type(parent),
    m_lockedValue(0),
    m_recordedValue(0)
{
    setStyleSheet(QLatin1String(stylesheet));
}

QnStorageSpaceWidget::~QnStorageSpaceWidget()
{

}

void QnStorageSpaceWidget::sliderChange(SliderChange change) {
    if (change == SliderValueChange) {
        if (value() < m_lockedValue)
            setValue(m_lockedValue);
    }
    base_type::sliderChange(change);
}

void QnStorageSpaceWidget::paintEvent(QPaintEvent *ev)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);

    QPainter p(this);
    /*QPainterPath path;

    int d = grooveRect.height();
    qreal r = d *0.15;
    path.addRoundedRect(grooveRect.adjusted(d, 0, -d, 0), r, r);
    p.fillPath(path, QBrush(QColor(Qt::red)));*/

    qreal w = grooveRect.width();
    qreal top = grooveRect.top();
    qreal height = grooveRect.height();
    int size = maximum();

    qreal lockedWidth = w * m_lockedValue / size;
    QRectF locked(grooveRect.left(), top, lockedWidth, height);

    qreal recordedWidth = w * (m_recordedValue - m_lockedValue) / size;
    QRectF recorded(locked.right(), top, recordedWidth, height);

    qreal reservedWidth = w * (value() - m_recordedValue) / size;
    QRectF reserved(recorded.right(), top, reservedWidth, height);

    p.fillRect(grooveRect, QBrush(QColor(Qt::white)));
    p.fillRect(locked, QBrush(QColor(Qt::gray)));
    p.fillRect(recorded, QBrush(QColor(Qt::red)));
    p.fillRect(reserved, QBrush(QColor(Qt::green)));

    QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    handleRect.setHeight(height*2);
    handleRect.setTop(top - height);
    const bool hovered = (opt.state & QStyle::State_Enabled) && (opt.activeSubControls & QStyle::SC_SliderHandle);

    QPixmap handlePic = qnSkin->pixmap(hovered ? "slider/slider_handle_hovered.png" : "slider/slider_handle.png", handleRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    p.drawPixmap(handleRect, handlePic);

  //  base_type::paintEvent(ev);
}
