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

#ifndef QSCROLLBAR_H
#define QSCROLLBAR_H

#include "abstract_graphics_slider.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

#ifndef QT_NO_SCROLLBAR

class QScrollBarPrivate;
class QStyleOptionSlider;

class Q_GUI_EXPORT GraphicsScrollBar : public AbstractGraphicsSlider
{
    Q_OBJECT
public:
    explicit GraphicsScrollBar(QWidget *parent=0);
    explicit GraphicsScrollBar(Qt::Orientation, QWidget *parent=0);
    ~GraphicsScrollBar();

    QSize sizeHint() const;
    bool event(QEvent *event);

protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void hideEvent(QHideEvent*);
    void sliderChange(SliderChange change);
#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(QContextMenuEvent *);
#endif
    void initStyleOption(QStyleOptionSlider *option) const;

#ifdef QT3_SUPPORT
public:
    QT3_SUPPORT_CONSTRUCTOR GraphicsScrollBar(QWidget *parent, const char* name);
    QT3_SUPPORT_CONSTRUCTOR GraphicsScrollBar(Qt::Orientation, QWidget *parent, const char* name);
    QT3_SUPPORT_CONSTRUCTOR GraphicsScrollBar(int minValue, int maxValue, int lineStep, int pageStep,
                int value, Qt::Orientation, QWidget *parent=0, const char* name = 0);
    inline QT3_SUPPORT bool draggingSlider() { return isSliderDown(); }
#endif

private:
    friend Q_GUI_EXPORT QStyleOptionSlider qt_qscrollbarStyleOption(GraphicsScrollBar *scrollBar);

    Q_DISABLE_COPY(GraphicsScrollBar)
    Q_DECLARE_PRIVATE(GraphicsScrollBar)
};

#endif // QT_NO_SCROLLBAR

QT_END_NAMESPACE

QT_END_HEADER

#endif // QSCROLLBAR_H
