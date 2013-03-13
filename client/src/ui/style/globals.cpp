#include "globals.h"

#include <cassert>

#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtCore/QStringList>

#include <QtGui/QFont>
#include <QtGui/QColor>

#include <utils/common/color.h>
#include <utils/common/module_resources.h>

#include "config.h"

Q_GLOBAL_STATIC(QnGlobals, qn_globalsInstance);

QnGlobals::QnGlobals(QObject *parent):
    base_type(parent)
{
    /* Ensure that default skin resource is loaded. 
     * This is needed because globals instance may be constructed before the
     * corresponding resource initializer is called. */
    QN_INIT_MODULE_RESOURCES(client);

    init();

    QString path = QLatin1String(QN_SKIN_PATH) + QLatin1String("/globals.ini");
    QScopedPointer<QSettings> settings(new QSettings(path, QSettings::IniFormat));
    settings->beginGroup(QLatin1String("style"));
    updateFromSettings(settings.data());
    settings->endGroup();
}

QnGlobals::~QnGlobals() {
    return;
}

QnGlobals *QnGlobals::instance() {
    return qn_globalsInstance();
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
