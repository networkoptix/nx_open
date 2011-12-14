#ifndef QN_CONVERTER_H
#define QN_CONVERTER_H

class QVariant;

/**
 * Abstract converter class. 
 */
class QnAbstractConverter {
public:
    virtual ~QnAbstractConverter() {}

    virtual QVariant convertTo(const QVariant &source) const = 0;

    virtual QVariant convertFrom(const QVariant &source) const = 0;
};


/**
 * QVector4D->QColor converter. 
 */
class QnVectorToColorConverter: public QnAbstractConverter {
public:
    virtual QVariant convertTo(const QVariant &source) const override;

    virtual QVariant convertFrom(const QVariant &source) const override;
};

#endif // QN_CONVERTER_H
