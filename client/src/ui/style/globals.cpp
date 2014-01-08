#include "globals.h"

#include <cassert>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtCore/QStringList>

#include <QtGui/QFont>
#include <QtGui/QColor>

#include <utils/common/json.h>

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

    Q_GLOBAL_STATIC(QnGlobals, qn_globals_instance);

} // anonymous namespace

QnGlobals::QnGlobals(QObject *parent):
    base_type(parent)
{
    /* Ensure that default skin resource is loaded. 
     * This is needed because globals instance may be constructed before the
     * corresponding resource initializer is called. */
    Q_INIT_RESOURCE(client);

    init();

    /* This is somewhat hacky. We still want the values to be writable for
     * customization engine to work, but we don't want the write accessors. */
    foreach(int id, variables())
        setWritable(id, true);

    QFile file(QLatin1String(":/skin") + QLatin1String("/globals.json")); // TODO: #Elric
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonObject jsonObject;
        if(!QJson::deserialize(file.readAll(), &jsonObject)) {
            qWarning() << "Client settings file could not be parsed!";
        } else {
            updateFromJson(jsonObject.value(QLatin1String("style")).toObject());
        }
    }
}

QnGlobals::~QnGlobals() {
    return;
}

QnGlobals *QnGlobals::instance() {
    return qn_globals_instance();
}

QVariant QnGlobals::readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) {
    int type = this->type(id);
    if(type == QMetaType::QColor) {
        return parseColor(settings->value(name(id)), defaultValue.value<QColor>());
    } else if (type == qMetaTypeId<QnStatisticsColors>()) {
        QnStatisticsColors colors;

        QVariant value = settings->value(name(id));
        QString serializedValue;
        if (value.type() == QVariant::String)
            serializedValue = value.toString();
        else if (value.type() == QVariant::StringList)
            serializedValue = value.value<QStringList>().join(QLatin1String(", "));

        if (!serializedValue.isEmpty())
            colors.update(serializedValue.toLatin1());
        return QVariant::fromValue<QnStatisticsColors>(colors);
    }
    else {
        return base_type::readValueFromSettings(settings, id, defaultValue);
    }
}

QVariant QnGlobals::readValueFromJson(const QJsonObject &json, int id, const QVariant &defaultValue) {
    int type = this->type(id);
    if(type == qMetaTypeId<QnStatisticsColors>()) {
        QnStatisticsColors value;
        if(QJson::deserialize(json.value(name(id)), &value)) {
            return QVariant::fromValue<QnStatisticsColors>(value);
        } else {
            return defaultValue;
        }
    } else {
        return base_type::readValueFromJson(json, id, defaultValue);
    }
}

QVector<QColor> QnGlobals::defaultZoomWindowColors() {
    return QVector<QColor>()
        << QColor(192, 32, 32)
        << QColor(32, 192, 32)
        << QColor(64, 64, 255);
}



