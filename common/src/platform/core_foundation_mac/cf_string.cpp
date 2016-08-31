
#include "cf_string.h"

cf::QnCFString::QnCFString()
    :
    base_type()
{
}

cf::QnCFString::QnCFString(CFStringRef ref)
    :
    base_type(ref)
{
}

cf::QnCFString::QnCFString(const QString& value)
    :
    base_type(value.toCFString())
{
}

cf::QnCFString::~QnCFString()
{
}

QString cf::QnCFString::toString() const
{
    return (ref() ? QString::fromCFString(ref()) : QString());
}
