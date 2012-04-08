#ifndef ABSTRACTGRAPHICSBUTTON_P_H
#define ABSTRACTGRAPHICSBUTTON_P_H

#include "graphicswidget_p.h"
#include "abstractgraphicsbutton.h"

#include <QtCore/QBasicTimer>
#include <QtCore/QList>

class AbstractGraphicsButtonPrivate: public GraphicsWidgetPrivate
{
    Q_DECLARE_PUBLIC(AbstractGraphicsButton)

public:
    AbstractGraphicsButtonPrivate(QSizePolicy::ControlType type = QSizePolicy::DefaultType);

    void init();
    void click();
    void refresh();

    QString text;
    QIcon icon;
    QSize iconSize;
#ifndef QT_NO_SHORTCUT
    QKeySequence shortcut;
    int shortcutId;
#endif
    uint checkable     : 1;
    uint checked       : 1;
    uint autoRepeat    : 1;
    uint autoExclusive : 1;
    uint down          : 1;
    uint blockRefresh  : 1;
    uint pressed       : 1;

#ifndef QT_NO_GRAPHICSBUTTONGROUP
    GraphicsButtonGroup *group;
#endif
    QBasicTimer repeatTimer;
    QBasicTimer animateTimer;

    int autoRepeatDelay, autoRepeatInterval;

    QSizePolicy::ControlType controlType;
    mutable QSizeF sizeHint;

    QList<AbstractGraphicsButton *> queryButtonList() const;
    AbstractGraphicsButton *queryCheckedButton() const;
    void notifyChecked();
    void moveFocus(int key);
    void fixFocusPolicy();

    void emitPressed();
    void emitReleased();
    void emitClicked();
};

#endif // ABSTRACTGRAPHICSBUTTON_P_H
