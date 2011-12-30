#ifndef cl_AssociativeArray_933
#define	cl_AssociativeArray_933

#include <QMap>

#include "base.h"

#include <math.h>

class QN_EXPORT QnValue
{
    QString value;
public:
    QnValue(){}
    QnValue(const QString& val) : value(val){}
    QnValue(const char* val) : value(QString::fromLatin1(val)){}

    QnValue(int val)
    {
        value.setNum(val);
    }

    QnValue(double val)
    {
        value.setNum(val);
    }

    operator bool() const { return value.toInt();}
    operator int() const { return value.toInt(); }
    operator double() const { return value.toDouble(); }
    operator QString() const { return value; }

    bool operator < ( const QnValue & other ) const
    {
        return value.toDouble() < (double)other;
    }

    bool operator > ( const QnValue & other ) const
    {
        return value.toDouble() > (double)other;
    }

    bool operator <= ( const QnValue & other ) const
    {
        return value.toDouble() <= (double)other;
    }

    bool operator >= ( const QnValue & other ) const
    {
        return value.toDouble() >= (double)other;
    }

    bool operator == ( const QnValue & other ) const
    {
        return value == other.value;
    }

    bool operator == ( int val ) const
    {
        return (fabs(value.toDouble() - val)) < 1e-6;
    }

    bool operator == ( double val ) const
    {
        return (fabs(value.toDouble() - val))< 1e-6;
    }

    bool operator == ( QString val ) const
    {
        return value == val;
    }

    bool operator != ( QString val ) const
    {
        return value != val;
    }

};

#endif //cl_AssociativeArray_933
