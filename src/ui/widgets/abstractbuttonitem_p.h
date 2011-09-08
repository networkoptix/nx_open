#ifndef ABSTRACTBUTTONITEM_P_H
#define ABSTRACTBUTTONITEM_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "abstractbuttonitem.h"

#include <QtCore/QBasicTimer>

class AbstractButtonItemPrivate
{
    Q_DECLARE_PUBLIC(AbstractButtonItem)

public:
    AbstractButtonItemPrivate(QSizePolicy::ControlType type = QSizePolicy::DefaultType);

    AbstractButtonItem *q_ptr;

    bool hovered;

    QString text;
    QIcon icon;
    QSize iconSize;
#ifndef QT_NO_SHORTCUT
    QKeySequence shortcut;
    int shortcutId;
#endif
    uint checkable :1;
    uint checked :1;
    uint autoRepeat :1;
    uint autoExclusive :1;
    uint down :1;
    uint blockRefresh :1;
    uint pressed : 1;

#ifndef QT_NO_BUTTONGROUP
    QButtonGroup* group;
#endif
    QBasicTimer repeatTimer;
    QBasicTimer animateTimer;

    int autoRepeatDelay, autoRepeatInterval;

    QSizePolicy::ControlType controlType;
    mutable QSize sizeHint;

    void init();
    void click();
    void refresh();

    QList<AbstractButtonItem *>queryButtonList() const;
    AbstractButtonItem *queryCheckedButton() const;
    void notifyChecked();
    void moveFocus(int key);
    void fixFocusPolicy();

    void emitPressed();
    void emitReleased();
    void emitClicked();
};

#endif // ABSTRACTBUTTONITEM_P_H
