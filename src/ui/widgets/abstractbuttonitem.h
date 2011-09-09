#ifndef ABSTRACTBUTTONITEM_H
#define ABSTRACTBUTTONITEM_H

#include <QtGui/QIcon>
#include <QtGui/QKeySequence>
#include <QtGui/QGraphicsWidget>

class QButtonGroup;

class AbstractButtonItemPrivate;
class AbstractButtonItem : public QGraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize)
#ifndef QT_NO_SHORTCUT
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut)
#endif
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked DESIGNABLE isCheckable NOTIFY toggled USER true)
    Q_PROPERTY(bool autoRepeat READ autoRepeat WRITE setAutoRepeat)
    Q_PROPERTY(bool autoExclusive READ autoExclusive WRITE setAutoExclusive)
    Q_PROPERTY(int autoRepeatDelay READ autoRepeatDelay WRITE setAutoRepeatDelay)
    Q_PROPERTY(int autoRepeatInterval READ autoRepeatInterval WRITE setAutoRepeatInterval)
    Q_PROPERTY(bool down READ isDown WRITE setDown DESIGNABLE false)

public:
    explicit AbstractButtonItem(QGraphicsItem *parent = 0);
    ~AbstractButtonItem();

    void setText(const QString &text);
    QString text() const;

    void setIcon(const QIcon &icon);
    QIcon icon() const;

    QSize iconSize() const;

#ifndef QT_NO_SHORTCUT
    void setShortcut(const QKeySequence &key);
    QKeySequence shortcut() const;
#endif

    void setCheckable(bool);
    bool isCheckable() const;

    bool isChecked() const;

    void setDown(bool);
    bool isDown() const;

    void setAutoRepeat(bool);
    bool autoRepeat() const;

    void setAutoRepeatDelay(int);
    int autoRepeatDelay() const;

    void setAutoRepeatInterval(int);
    int autoRepeatInterval() const;

    void setAutoExclusive(bool);
    bool autoExclusive() const;

#ifndef QT_NO_BUTTONGROUP
    QButtonGroup *group() const;
#endif

public Q_SLOTS:
    void setIconSize(const QSize &size);
    void animateClick(int msec = 100);
    void click();
    void toggle();
    void setChecked(bool);

Q_SIGNALS:
    void pressed();
    void released();
    void clicked(bool checked = false);
    void toggled(bool checked);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) = 0;
    virtual bool hitButton(const QPointF &pos) const;
    virtual void checkStateSet();
    virtual void nextCheckState();

    void initStyleOption(QStyleOption *option) const;

    bool event(QEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void keyReleaseEvent(QKeyEvent *e);
    void mousePressEvent(QGraphicsSceneMouseEvent * event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e);
    void focusInEvent(QFocusEvent *e);
    void focusOutEvent(QFocusEvent *e);
    void changeEvent(QEvent *e);
    void timerEvent(QTimerEvent *e);

protected:
    AbstractButtonItem(AbstractButtonItemPrivate &dd, QGraphicsItem *parent = 0);
    const QScopedPointer<AbstractButtonItemPrivate> d_ptr;

private:
    Q_DISABLE_COPY(AbstractButtonItem)
    Q_DECLARE_PRIVATE(AbstractButtonItem)
    friend class QButtonGroup;
};

#endif // ABSTRACTBUTTONITEM_H
