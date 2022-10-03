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

#ifndef GRAPHICSLABEL_H
#define GRAPHICSLABEL_H

#include "graphics_frame.h"

#include <QtGui/QStaticText>

class GraphicsLabelPrivate;

class GraphicsLabel : public GraphicsFrame
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_ENUMS(PerformanceHint)
    typedef GraphicsFrame base_type;

public:
    enum PerformanceHint {
        NoCaching,
        ModerateCaching,
        AggressiveCaching,
        PixmapCaching
    };

    explicit GraphicsLabel(QGraphicsItem* parent = nullptr, Qt::WindowFlags flags = {});
    explicit GraphicsLabel(
        const QString& text, QGraphicsItem* parent = nullptr, Qt::WindowFlags flags = {});
    ~GraphicsLabel();

    QString text() const;
    Q_SLOT void setText(const QString &text);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    PerformanceHint performanceHint() const;
    void setPerformanceHint(PerformanceHint performanceHint);

    void setShadowRadius(qreal radius);

    Qt::TextElideMode elideMode() const;
    void setElideMode(Qt::TextElideMode elideMode);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

public slots:
    void setNum(int num) { setText(QString::number(num)); }
    void setNum(double num) { setText(QString::number(num)); }
    void clear();

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

    virtual void changeEvent(QEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    Q_DISABLE_COPY(GraphicsLabel)
    Q_DECLARE_PRIVATE(GraphicsLabel)
};

#endif // GRAPHICSLABEL_H
