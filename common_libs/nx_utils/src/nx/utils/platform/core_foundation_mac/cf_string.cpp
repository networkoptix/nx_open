
#include "cf_string.h"

cf::QnCFString::QnCFString()
    :
    base_type()
{
}

cf::QnCFString::QnCFString(const QnCFString& other)
    : base_type(other)
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

cf::QnCFString cf::QnCFString::makeOwned(CFStringRef ref)
{
    return base_type::makeOwned<QnCFString, CFStringRef>(ref);
}
