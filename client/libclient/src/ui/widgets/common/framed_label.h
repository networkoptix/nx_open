#ifndef FRAMED_LABEL_H
#define FRAMED_LABEL_H

#include <QtWidgets/QLabel>

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
 */
class QnFramedLabel: public QLabel {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
    typedef QLabel base_type;

public:
    explicit QnFramedLabel(QWidget *parent = NULL);
    virtual ~QnFramedLabel();

    /** 
     * \returns                         Size of label's contents (without frame).
     */
    QSize contentSize() const;

    qreal opacity() const;
    void setOpacity(qreal value);

    QColor frameColor() const;
    void setFrameColor(const QColor color);

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    qreal m_opacity;
};

#endif // FRAMED_LABEL_H
