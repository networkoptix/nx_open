#ifndef QN_COLOR_TO_VECTOR_CONVERTER_H
#define QN_COLOR_TO_VECTOR_CONVERTER_H

#include "converter.h"

#include <QtGui/QVector4D>
#include <QtGui/QColor>

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
