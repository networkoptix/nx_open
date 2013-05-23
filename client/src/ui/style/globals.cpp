#include "globals.h"

#include <cassert>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtCore/QStringList>

#include <QtGui/QFont>
#include <QtGui/QColor>

#include <utils/common/color.h>
#include <utils/common/module_resources.h>

#include "config.h"

Q_GLOBAL_STATIC(QnGlobals, qn_globals_instance);

QnGlobals::QnGlobals(QObject *parent):
    base_type(parent)
{
    /* Ensure that default skin resource is loaded. 
     * This is needed because globals instance may be constructed before the
     * corresponding resource initializer is called. */
    QN_INIT_MODULE_RESOURCES(client);

    init();

    QFile file(QLatin1String(QN_SKIN_PATH) + QLatin1String("/globals.json"));
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QVariantMap json;
        if(!QJson::deserialize(file.readAll(), &json)) {
            qWarning() << "Client settings file could not be parsed!";
        } else {
            updateFromJson(json.value(lit("style")).toMap());
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

QVariant QnGlobals::readValueFromJson(const QVariantMap &json, int id, const QVariant &defaultValue) {
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



