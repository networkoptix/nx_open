#ifndef FRAMED_LABEL_H
#define FRAMED_LABEL_H

#include <QLabel>

class QPaintDelegate {
public:
    virtual void delegatedPaint(const QWidget* widget, QPainter *painter) = 0;
};

/**
 * The QnFramedLabel class implements the label control that can draw frame around
 * the pixmap. If pixmap is not set, it is painted as a usual QLabel.
 * Standard QLabel has two drawbacks:
 *  - the frame will not be displayed around the pixmap
 *  - color of the frame is the same as text color
 *
 * QnFramedLabel also supports opacity setup.
 *
 * Width of the frame is controlled through standard property lineWidth().
 * Frame is not painted if lineWidth() is 0.
 *
 */
class QnFramedLabel: public QLabel {
    typedef QLabel base_type;

    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)

public:
    explicit QnFramedLabel(QWidget* parent = 0);
    virtual ~QnFramedLabel();

    /** \reimp
     * Label size without frame.
     */
    QSize size() const;

    qreal opacity() const;
    void setOpacity(qreal value);

    QColor frameColor() const;
    void setFrameColor(const QColor color);

    void addPaintDelegate(QPaintDelegate *delegate);
protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    qreal m_opacity;
    QColor m_frameColor;
    QList<QPaintDelegate*> m_paintDelegates; //minor(?) hack --gdm
};

#endif // FRAMED_LABEL_H
