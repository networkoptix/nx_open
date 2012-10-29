#ifndef __BUSINESS_LOGIC_COMMON_H__
#define __BUSINESS_LOGIC_COMMON_H__

#include <QString>
#include <QVariant>

QByteArray serializeBusinessParams(const QnBusinessParams& value);
QnBusinessParams deserializeBusinessParams(const QByteArray& value);

enum ToggleState {ToggleState_Off = 0, ToggleState_On = 1, ToggleState_NotDefined};
typedef QMap<QString, QVariant> QnBusinessParams; // param name and param value


#endif // __BUSINESS_LOGIC_COMMON_H__
