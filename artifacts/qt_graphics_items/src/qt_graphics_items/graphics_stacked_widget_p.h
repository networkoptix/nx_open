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

#pragma once

#include <QtCore/QList>

#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QStackedLayout>

class QnGraphicsStackedWidget;
class QnGraphicsStackedWidgetPrivate: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_DECLARE_PUBLIC(QnGraphicsStackedWidget);
    QnGraphicsStackedWidget* const q_ptr;

public:
    QnGraphicsStackedWidgetPrivate(QnGraphicsStackedWidget* main);
    virtual ~QnGraphicsStackedWidgetPrivate();

    int insertWidget(int index, QGraphicsWidget* widget);
    QGraphicsWidget* removeWidget(int index);
    int removeWidget(QGraphicsWidget* widget);

    int count() const;
    int indexOf(QGraphicsWidget* widget) const;
    QGraphicsWidget* widgetAt(int index) const;

    Qt::Alignment alignment(QGraphicsWidget* widget) const;
    void setAlignment(QGraphicsWidget* widget, Qt::Alignment alignment);

    int currentIndex() const;
    QGraphicsWidget* setCurrentIndex(int index);
    QGraphicsWidget* currentWidget() const;
    int setCurrentWidget(QGraphicsWidget* widget);

    QStackedLayout::StackingMode stackingMode() const;
    void setStackingMode(QStackedLayout::StackingMode mode);

    QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint) const;

private:
    void updateVisibility();
    void updateGeometries();

    void setWidgetGeometry(QGraphicsWidget* widget, const QRectF& geometry);

private:
    QList<QGraphicsWidget*> m_widgets;
    QHash<QGraphicsWidget*, Qt::Alignment> m_alignments;

    int m_currentIndex = -1;
    QStackedLayout::StackingMode m_stackingMode = QStackedLayout::StackOne;
};
