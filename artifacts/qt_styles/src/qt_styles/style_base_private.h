// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
**
****************************************************************************/

#pragma once

#include <QtWidgets/private/qcommonstyle_p.h>

class StyleBasePrivate: public QCommonStylePrivate
{
    Q_DECLARE_PUBLIC(QCommonStyle)

public:
    explicit StyleBasePrivate();
    virtual ~StyleBasePrivate() override;

    static int sliderPositionFromValue(
        int min, int max, int logicalValue, int span, bool upsideDown = false);

    static void drawArrowIndicator(
        const QStyle* style,
        const QStyleOptionToolButton* toolbutton,
        const QRect& rect,
        QPainter* painter,
        const QWidget* widget = nullptr);

    void drawToolButton(
        QPainter* painter,
        const QStyleOptionToolButton* option,
        const QWidget* widget = nullptr) const;

    void drawSliderTickmarks(
        QPainter* painter,
        const QStyleOptionComplex* option,
        const QWidget* widget = nullptr) const;
};
