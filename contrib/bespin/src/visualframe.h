/* Bespin widget style for Qt4
   Copyright (C) 2007 Thomas Luebking <thomas.luebking@web.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#ifndef VISUALFRAME_H
#define VISUALFRAME_H

class QFrame;
class QMouseEvent;
class QWheelEvent;
class QPaintEvent;

#include <QWidget>
#include <QPoint>
#include <QPointer>
#include <QFrame>

namespace VFrame
{
enum Side { North, South, West, East };
enum Type {Sunken, Plain, Raised};
}

class VisualFrame;
class VisualFramePart : public QWidget
{
    Q_OBJECT
public:
    VisualFramePart(QWidget *window, QFrame *parent, VisualFrame *vFrame, VFrame::Side side);
    VisualFramePart(){};
    inline const QFrame *frame() const { return myFrame; }
protected:
//    void enterEvent ( QEvent * event ) { passDownEvent(event, event->globalPos()); }
//    void leaveEvent ( QEvent * event ) { passDownEvent(event, event->globalPos()); }
    void mouseDoubleClickEvent ( QMouseEvent * event );
    void mouseMoveEvent(QMouseEvent*);
    void mousePressEvent(QMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);
    void paintEvent(QPaintEvent*);
    void wheelEvent(QWheelEvent*);
private:
    Q_DISABLE_COPY(VisualFramePart)
    QFrame *myFrame; // parent, to avoid nasty casting
    VisualFrame *_vFrame;
    VFrame::Side _side;
    void passDownEvent(QEvent *ev, const QPoint &gMousePos);
};

class VisualFrame : public QObject
{
    Q_OBJECT
public:
    inline const QRect &frameRect() const { return myFrameRect; }
    static void setGeometry(QFrame::Shadow shadow, const QRect &inner, const QRect &outer);
    static bool manage(QFrame *frame);
    static void release(QFrame *frame);
    static void setStyle(QStyle *style) { ourStyle = style; };
    inline static QStyle *style() { return ourStyle; };
public slots:
    void show();
    void hide();
    void raise();
    void update();
    void repaint();
protected:
    bool eventFilter(QObject*, QEvent*);
private:
    Q_DISABLE_COPY(VisualFrame)
    VisualFrame( QFrame *parent );
//    ~VisualFrame();
    void updateShape();
    QFrame *myFrame; // parent
    QWidget *myWindow;
    QFrame::Shape myStyle;
    QPointer<VisualFramePart> top, bottom, left, right;
    QRect myFrameRect;
    bool hidden;
    static QStyle *ourStyle;
private slots:
    void correctPosition();
    void deleteMuchLater();
    void hideMe();
};

#endif //VISUALFRAME_H
