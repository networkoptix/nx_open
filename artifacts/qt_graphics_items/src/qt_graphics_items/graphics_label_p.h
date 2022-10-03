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

#ifndef GRAPHICSLABEL_P_H
#define GRAPHICSLABEL_P_H

#include "graphics_frame_p.h"
#include "graphics_label.h"

class QGraphicsSimpleTextItem;

class GraphicsLabelPrivate : public GraphicsFramePrivate
{
    Q_DECLARE_PUBLIC(GraphicsLabel)

public:
    void init();

    void updateSizeHint();

    void ensurePixmaps();
    void ensureStaticText();

    QColor textColor() const;
    QString calculateText() const;

    GraphicsLabel::PerformanceHint performanceHint;
    Qt::Alignment alignment;
    QString text;
    QStaticText staticText;
    QPixmap pixmap;
    QSizeF minimumSize;
    QSizeF preferredSize;
    QSizeF cachedSize;
    bool staticTextDirty;
    bool pixmapDirty;
    qreal shadowRadius = 0.0;
    Qt::TextElideMode elideMode = Qt::ElideNone;
};

#endif // GRAPHICSLABEL_P_H
