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

#ifndef ABSTRACTLINEARGRAPHICSSLIDER_H
#define ABSTRACTLINEARGRAPHICSSLIDER_H

#include "abstract_graphics_slider.h"

class AbstractLinearGraphicsSliderPrivate;

class AbstractLinearGraphicsSlider: public AbstractGraphicsSlider {
    Q_OBJECT;

    typedef AbstractGraphicsSlider base_type;

public:
    explicit AbstractLinearGraphicsSlider(QGraphicsItem *parent = 0);
    virtual ~AbstractLinearGraphicsSlider();

    virtual QPointF positionFromValue(qint64 logicalValue, bool bound = true) const override;
    virtual qint64 valueFromPosition(const QPointF &position, bool bound = true) const override;

    virtual void updateGeometry() override;

protected:
    AbstractLinearGraphicsSlider(AbstractLinearGraphicsSliderPrivate &dd, QGraphicsItem *parent);

    virtual bool event(QEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

    virtual void sliderChange(SliderChange change) override;

private:
    Q_DECLARE_PRIVATE(AbstractLinearGraphicsSlider);
};



#endif // ABSTRACTLINEARGRAPHICSSLIDER_H
