#ifndef QN_CONVERTER_H
#define QN_CONVERTER_H

#include <typeinfo>

#include <utils/common/warnings.h>


class QVariant;

template<class Target, class Source>
void convert(const Source &source, Target *target) {
    qnWarning("convert function is not implemented for source type '%1' and target type '%2'.", typeid(Source).name(), typeid(Target).name());
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
    AbstractConverter(int sourceType, int targetType):
        m_sourceType(sourceType),
        m_targetType(targetType)
    {}

    int sourceType() const {
        return m_sourceType;
    }

    int targetType() const {
        return m_targetType;
    }

    QVariant convertSourceToTarget(const QVariant &source) const;

    QVariant convertTargetToSource(const QVariant &target) const;

    virtual ~AbstractConverter() {}

protected:
    virtual QVariant doConvertSourceToTarget(const QVariant &source) const = 0;

    virtual QVariant doConvertTargetToSource(const QVariant &target) const = 0;

private:
    int m_sourceType;
    int m_targetType;
};

#endif // QN_CONVERTER_H
