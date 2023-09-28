// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_COLOR_TO_VECTOR_CONVERTER_H
#define QN_COLOR_TO_VECTOR_CONVERTER_H

#include <QtGui/QColor>
#include <QtGui/QVector4D>

#include "converter.h"

void convert(const QVector4D &source, QColor *target);
void convert(const QColor &source, QVector4D *target);

/**
 * QVector4D -> QColor converter.
 */
class QnColorToVectorConverter: public AbstractConverter {
public:
    QnColorToVectorConverter();

protected:
    virtual QVariant doConvertSourceToTarget(const QVariant &source) const override;

    virtual QVariant doConvertTargetToSource(const QVariant &target) const override;
};

#endif // QN_COLOR_TO_VECTOR_CONVERTER_H
