/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef ABSTRACTBUTTONITEM_H
#define ABSTRACTBUTTONITEM_H

#include <QtGui/qicon.h>
#include <QtGui/qkeysequence.h>
#include <QtGui/qgraphicswidget.h>
#include <QPainter>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class QButtonGroup;
class AbstractButtonItemPrivate;

class Q_GUI_EXPORT AbstractButtonItem : public QGraphicsWidget
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
    explicit AbstractButtonItem(QGraphicsWidget *parent=0);
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
//    virtual void paintEvent(QPaintEvent *e) = 0;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) { painter->fillRect(boundingRect(), Qt::black); }
    virtual bool hitButton(const QPointF &pos) const;
    virtual void checkStateSet();
    virtual void nextCheckState();

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
    AbstractButtonItem(AbstractButtonItemPrivate &dd, QGraphicsWidget* parent = 0);

private:
    Q_DECLARE_PRIVATE(AbstractButtonItem)
    Q_DISABLE_COPY(AbstractButtonItem)
    friend class QButtonGroup;

    AbstractButtonItemPrivate *d_ptr;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // ABSTRACTBUTTONITEM_H
