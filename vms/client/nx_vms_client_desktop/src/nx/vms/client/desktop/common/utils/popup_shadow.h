#pragma once

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtGui/QColor>

class QWidget;
class QEvent;

namespace nx::vms::client::desktop {

/**
 * Common class to display a shadow cast by a rectangular popup widget.
 *  Shadow is implemented as another semi-transparent popup widget.
 */
class PopupShadow: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    /** Shadow color and transparency: */
    Q_PROPERTY(QColor color READ color WRITE setColor)

    /** Shadow offset: */
    Q_PROPERTY(QPoint offset READ offset WRITE setOffset)

    /** Shadow blur radius: */
    Q_PROPERTY(int blurRadius READ blurRadius WRITE setBlurRadius)

    /** Shadow extra size added to each edge: */
    Q_PROPERTY(int spread READ spread WRITE setSpread)

public:
    explicit PopupShadow(QWidget* popup);
    virtual ~PopupShadow();

    QColor color() const;
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
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
