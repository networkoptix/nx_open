// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_CONVERTER_H
#define QN_CONVERTER_H

#include <typeinfo>

#include <nx/utils/log/log.h>

class QVariant;

template<class Target, class Source>
void convert(const Source& /*source*/, Target* /*target*/) {
    NX_ASSERT(false,
        "convert function is not implemented for source type '%1' and target type '%2'.",
        typeid(Source).name(),
        typeid(Target).name());
}

template<class Target, class Source>
Target convert(const Source &source) {
    using ::convert;

    Target result;
    convert(source, &result);
    return result;
}


/**
 * Abstract converter class.
 */
class AbstractConverter {
public:
    AbstractConverter(QMetaType sourceType, QMetaType targetType):
        m_sourceType(sourceType),
        m_targetType(targetType)
    {}

    QMetaType sourceType() const { return m_sourceType; }

    QMetaType targetType() const { return m_targetType; }

    QVariant convertSourceToTarget(const QVariant &source) const;

    QVariant convertTargetToSource(const QVariant &target) const;

    virtual ~AbstractConverter() {}

protected:
    virtual QVariant doConvertSourceToTarget(const QVariant &source) const = 0;

    virtual QVariant doConvertTargetToSource(const QVariant &target) const = 0;

private:
    QMetaType m_sourceType;
    QMetaType m_targetType;
};

#endif // QN_CONVERTER_H
