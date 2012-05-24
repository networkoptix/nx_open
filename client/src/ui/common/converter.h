#ifndef QN_CONVERTER_H
#define QN_CONVERTER_H

class QVariant;

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


/**
 * QVector4D->QColor converter. 
 */
class QnColorToVectorConverter: public AbstractConverter {
public:
    QnColorToVectorConverter();

protected:
    virtual QVariant doConvertSourceToTarget(const QVariant &source) const override;

    virtual QVariant doConvertTargetToSource(const QVariant &target) const override;
};

#endif // QN_CONVERTER_H
