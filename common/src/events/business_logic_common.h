#ifndef __BUSINESS_LOGIC_COMMON_H__
#define __BUSINESS_LOGIC_COMMON_H__

#include <QString>
#include <QVariant>


namespace ToggleState
{
    enum Value
    {
        Off = 0,
        On = 1,
        //!Used in event rule to associate non-toggle action with event with any toggle state
        Any,
        NotDefined,

        /** Used to implement for-loops over the enumeration */
        Count
    };

    QString toString(Value val);

}
typedef QMap<QString, QVariant> QnBusinessParams; // param name and param value

QByteArray serializeBusinessParams(const QnBusinessParams& value);
QnBusinessParams deserializeBusinessParams(const QByteArray& value);

#endif // __BUSINESS_LOGIC_COMMON_H__
