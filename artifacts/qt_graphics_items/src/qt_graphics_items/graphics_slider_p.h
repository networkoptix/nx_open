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

#ifndef GRAPHICSSLIDER_P_H
#define GRAPHICSSLIDER_P_H

#include "abstract_linear_graphics_slider_p.h"
#include "graphics_slider.h"

class GraphicsSliderPrivate : public AbstractLinearGraphicsSliderPrivate
{
    Q_DECLARE_PUBLIC(GraphicsSlider)

public:
    QStyle::SubControl pressedControl;
    GraphicsSlider::TickPosition tickPosition;
    qint64 tickInterval;
    QPoint clickOffset;

    void init();
    int pick(const QPoint &pt) const { return orientation == Qt::Horizontal ? pt.x() : pt.y(); }

    QStyle::SubControl newHoverControl(const QPoint &pos);
    void updateHoverControl(const QPoint &pos);
    QStyle::SubControl hoverControl;
    QRect hoverRect;
};

#endif // GRAPHICSSLIDER_P_H
