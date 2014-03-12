#ifndef QN_STORAGE_SPACE_SLIDER_H
#define QN_STORAGE_SPACE_SLIDER_H

#include <QtWidgets/QSlider>

#include <utils/common/string.h>
#include <utils/math/color_transformations.h>
#include <ui/style/noptix_style.h>


/**
 * A slider that displays the space to be used on a storage. 
 * Slider value is expected to be in MiBs.
 *
 * This slider was used as an editor widget in server settings dialog.
 * Currently it is not used anywhere, but we want to keep it.
 */
class QnStorageSpaceSlider: public QSlider {
    Q_OBJECT
    typedef QSlider base_type;

public:
    QnStorageSpaceSlider(QWidget *parent = NULL):
        base_type(parent),
        m_color(Qt::white)
    {
        setOrientation(Qt::Horizontal);
        setMouseTracking(true);
        setProperty(Qn::SliderLength, 0);

        setTextFormat(QLatin1String("%1"));

        connect(this, SIGNAL(sliderPressed()), this, SLOT(update()));
        connect(this, SIGNAL(sliderReleased()), this, SLOT(update()));
    }

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color) {
        m_color = color;
    }

    QString text() const {
        const qint64 bytesInMiB = 1024 * 1024;

        if(!m_textFormatHasPlaceholder) {
            return m_textFormat;
        } else {
            if(isSliderDown()) {
                return formatSize(sliderPosition() * bytesInMiB);
            } else {
                return tr("%1%").arg(static_cast<int>(relativePosition() * 100));
            }
        }
    }

    QString textFormat() const {
        return m_textFormat;
    }

    void setTextFormat(const QString &textFormat) {
        if(m_textFormat == textFormat)
            return;

        m_textFormat = textFormat;
        m_textFormatHasPlaceholder = textFormat.contains(QLatin1String("%1"));
        update();
    }

    static QString formatSize(qint64 size) {
        return formatFileSize(size, 1, 10);
    }

protected:
    virtual void mouseMoveEvent(QMouseEvent *event) override {
        base_type::mouseMoveEvent(event);

        if(!isEmpty()) {
            int x = handlePos();
            if(qAbs(x - event->pos().x()) < 6) {
                setCursor(Qt::SplitHCursor);
            } else {
                unsetCursor();
            }
        }
    }

    virtual void leaveEvent(QEvent *event) override {
        unsetCursor();

        base_type::leaveEvent(event);
    }

    virtual void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        QRect rect = this->rect();

        painter.fillRect(rect, palette().color(backgroundRole()));

        if(!isEmpty()) {
            int x = handlePos();
            painter.fillRect(QRect(QPoint(0, 0), QPoint(x, rect.bottom())), m_color);

            painter.setPen(withAlpha(m_color.lighter(), 128));
            painter.drawLine(QPoint(x, 0), QPoint(x, rect.bottom()));
        }

        const int textMargin = style()->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, this) + 1;
        QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }

private:
    qreal relativePosition() const {
        return isEmpty() ? 0.0 : static_cast<double>(sliderPosition() - minimum()) / (maximum() - minimum());
    }

    int handlePos() const {
        return rect().width() * relativePosition();
    }

    bool isEmpty() const {
        return maximum() == minimum();
    }

private:
    QColor m_color;
    QString m_textFormat;
    bool m_textFormatHasPlaceholder;
};

#endif // QN_STORAGE_SPACE_SLIDER_H
