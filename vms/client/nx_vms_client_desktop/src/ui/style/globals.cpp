#include "globals.h"

#include <cassert>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtCore/QStringList>

#include <QtGui/QFont>
#include <QtGui/QColor>

#include <nx/fusion/serialization/lexical_functions.h>

namespace {
    QColor parseColor(const QVariant &value, const QColor &defaultValue) {
        if(value.userType() == QVariant::Color) {
            return value.value<QColor>();
        } else if(value.userType() == QVariant::StringList) {
            QColor result;
            return QnLexical::deserialize(value.toStringList().join(QLatin1String(",")), &result) ? result : defaultValue;
        } else {
            QColor result;
            return QnLexical::deserialize(value.toString(), &result) ? result : defaultValue;
        }
    }

} // anonymous namespace

QnGlobals::QnGlobals(QObject *parent):
    base_type(parent)
{
    init();

    /* This is somewhat hacky. We still want the values to be writable for
     * customization engine to work, but we don't want the write accessors. */
    foreach(int id, variables())
        setWritable(id, true);
}

QnGlobals::~QnGlobals() {
    return;
}

QVariant QnGlobals::readValueFromSettings(QSettings *settings, int id,
    const QVariant& defaultValue) const
{
    int type = this->type(id);
    if(type == QMetaType::QColor) {
        return parseColor(settings->value(name(id)), defaultValue.value<QColor>());
    } else {
        return base_type::readValueFromSettings(settings, id, defaultValue);
    }
}

QVariant QnGlobals::readValueFromJson(const QJsonObject &json, int id, const QVariant &defaultValue) {
    return base_type::readValueFromJson(json, id, defaultValue);
}



