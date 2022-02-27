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

#ifndef GRAPHICSFRAME_P_H
#define GRAPHICSFRAME_P_H

#include "graphics_widget_p.h"
#include "graphics_frame.h"

class GraphicsFramePrivate: public GraphicsWidgetPrivate
{
    Q_DECLARE_PUBLIC(GraphicsFrame)

public:
    GraphicsFramePrivate()
        : GraphicsWidgetPrivate(),
          frect(QRect(0, 0, 0, 0)),
          frameStyle(GraphicsFrame::NoFrame | GraphicsFrame::Plain),
          lineWidth(1),
          midLineWidth(0),
          frameWidth(0),
          leftFrameWidth(0), rightFrameWidth(0), topFrameWidth(0), bottomFrameWidth(0)
    {}

    void updateFrameWidth();
    void updateStyledFrameWidths();

    QRect frect;
    int frameStyle;
    short lineWidth;
    short midLineWidth;
    short frameWidth;
    short leftFrameWidth, rightFrameWidth, topFrameWidth, bottomFrameWidth;
};

#endif // GRAPHICSFRAME_P_H
