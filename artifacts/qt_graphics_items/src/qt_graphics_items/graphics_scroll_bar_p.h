// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 1992-2005 Trolltech AS. All rights reserved.
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef GRAPHICSSCROLLBAR_P_H
#define GRAPHICSSCROLLBAR_P_H

#include "abstract_linear_graphics_slider_p.h"

class GraphicsScrollBarPrivate : public AbstractLinearGraphicsSliderPrivate
{
    Q_DECLARE_PUBLIC(GraphicsScrollBar)
public:
    QStyle::SubControl pressedControl;
    bool pointerOutsidePressedControl;
    bool adjustForPressedHandle;

    QPointF relativeClickOffset;
    qint64 snapBackPosition;

    void activateControl(uint control, int threshold = 500);
    void stopRepeatAction();
    void init();
    bool updateHoverControl(const QPointF &pos);
    QStyle::SubControl newHoverControl(const QPointF &pos);

    QStyle::SubControl hoverControl;
    QRect hoverRect;

    void setSliderPositionIgnoringAdjustments(qint64 position);
};

#endif // GRAPHICSSCROLLBAR_P_H

