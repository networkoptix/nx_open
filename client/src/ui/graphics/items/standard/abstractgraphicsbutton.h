#ifndef ABSTRACTGRAPHICSBUTTON_H
#define ABSTRACTGRAPHICSBUTTON_H

#include <QtGui/QIcon>
#include <QtGui/QKeySequence>
#include <QtGui/QGraphicsWidget>

#ifdef QT_NO_BUTTONGROUP
#  define QT_NO_GRAPHICSBUTTONGROUP
#else
// ### do not use button group for now
#  define QT_NO_GRAPHICSBUTTONGROUP
class GraphicsButtonGroup;
#endif

class AbstractGraphicsButtonPrivate;
class AbstractGraphicsButton : public QGraphicsWidget
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
    explicit AbstractGraphicsButton(QGraphicsItem *parent = 0);
    ~AbstractGraphicsButton();

    QString text() const;
    void setText(const QString &text);

    QIcon icon() const;
    void setIcon(const QIcon &icon);

    QSize iconSize() const;
    inline void setIconSize(int w, int h)
    { setIconSize(QSize(w, h)); }

#ifndef QT_NO_SHORTCUT
    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence &key);
#endif

    bool isCheckable() const;
    void setCheckable(bool);

    bool isChecked() const;

    bool isDown() const;
    void setDown(bool);

    bool autoRepeat() const;
    void setAutoRepeat(bool);

    int autoRepeatDelay() const;
    void setAutoRepeatDelay(int);

    int autoRepeatInterval() const;
    void setAutoRepeatInterval(int);

    bool autoExclusive() const;
    void setAutoExclusive(bool);

#ifndef QT_NO_GRAPHICSBUTTONGROUP
    GraphicsButtonGroup *group() const;
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
    virtual bool hitButton(const QPointF &pos) const;
    virtual void checkStateSet();
    virtual void nextCheckState();

    bool event(QEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void keyReleaseEvent(QKeyEvent *e);
    void mousePressEvent(QGraphicsSceneMouseEvent *e);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e);
    void focusInEvent(QFocusEvent *e);
    void focusOutEvent(QFocusEvent *e);
    void changeEvent(QEvent *e);
    void timerEvent(QTimerEvent *e);

protected:
    AbstractGraphicsButton(AbstractGraphicsButtonPrivate &dd, QGraphicsItem *parent);

private:
    Q_DISABLE_COPY(AbstractGraphicsButton)
    Q_DECLARE_PRIVATE(AbstractGraphicsButton)
    friend class GraphicsButtonGroup;
};

#endif // ABSTRACTGRAPHICSBUTTON_H
