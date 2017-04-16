#pragma once

#include <QtCore/QObject>

class QnPopupShadowPrivate;

/**
 * Common class to display a shadow cast by a rectangular popup widget.
 *  Shadow is implemented as another semi-transparent popup widget.
 */
class QnPopupShadow : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

    /** Shadow color and transparency: */
    Q_PROPERTY(QColor color READ color WRITE setColor)

    /** Shadow offset: */
    Q_PROPERTY(QPoint offset READ offset WRITE setOffset)

    /** Shadow blur radius: */
    Q_PROPERTY(int blurRadius READ blurRadius WRITE setBlurRadius)

    /** Shadow extra size added to each edge: */
    Q_PROPERTY(int spread READ spread WRITE setSpread)

public:
    explicit QnPopupShadow(QWidget* popup);
    virtual ~QnPopupShadow();

    const QColor& color() const;
    void setColor(const QColor& color);

    QPoint offset() const;
    void setOffset(const QPoint& offset);
    void setOffset(int x, int y);

    int blurRadius() const;
    void setBlurRadius(int blurRadius);

    int spread() const;
    void setSpread(int spread);

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    QScopedPointer<QnPopupShadowPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnPopupShadow)
};
