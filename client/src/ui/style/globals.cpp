#include "globals.h"

#include <cassert>

#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtGui/QFont>
#include <QtGui/QColor>

#include "config.h"

Q_GLOBAL_STATIC(QnGlobals, qn_globalsInstance);

namespace {
    QColor parseColor(const QString &name, const QColor &defaultValue = QColor()) {
        QString trimmedName = name.trimmed();
        if(trimmedName.startsWith(QLatin1String("QColor"))) {
            trimmedName = trimmedName.mid(trimmedName.indexOf(QLatin1Char('(')) + 1);
            trimmedName = trimmedName.left(trimmedName.lastIndexOf(QLatin1Char(')')));

            QStringList args = trimmedName.split(QLatin1Char(','));
            if(args.size() < 3 || args.size() > 4)
                return defaultValue;

            QList<int> colors;
            foreach(const QString &arg, args) {
                bool ok = false;
                int color = arg.toInt(&ok);
                if(!ok)
                    return defaultValue;
                colors.push_back(color);
            }

            QColor result(colors[0], colors[1], colors[2]);
            if(colors.size() == 4)
                result.setAlpha(colors[3]);

            return result;
        } else {
            QColor result(trimmedName);
            if(result.isValid()) {
                return result;
            } else {
                return defaultValue;
            }
        }
    }

    QColor parseColor(const QVariant &value, const QColor &defaultValue = QColor()) {
        if(value.userType() == QVariant::Color) {
            return value.value<QColor>();
        } else if(value.userType() == QVariant::StringList) {
            return parseColor(value.toStringList().join(QLatin1String(",")), defaultValue);
        } else {
            return parseColor(value.toString(), defaultValue);
        }
    }

} // anonymous namespace


QnGlobals::QnGlobals(QObject *parent):
    base_type(parent)
{
    /* Ensure that default skin resource is loaded. 
     * This is needed because globals instance may be constructed before the
     * corresponding resource initializer is called. */
    Q_INIT_RESOURCE(skin);

    init();

    QString path = QString(QN_SKIN_PATH) + QLatin1String("/skin/globals.ini");
    QScopedPointer<QSettings> settings(new QSettings(path, QSettings::IniFormat));
    settings->beginGroup(QLatin1String("globals"));
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
    } else {
        return base_type::readValueFromSettings(settings, id, defaultValue);
    }
}

QnColorVector QnGlobals::initSystemHealthColors(){
    QnColorVector result;
    result.append(QColor(66, 140, 237));
    result.append(QColor(237, 66, 138));
    result.append(QColor(237, 200, 66));
    result.append(QColor(103, 237, 66));
    result.append(QColor(237, 237, 237));
    result.append(QColor(59, 169, 219));
    result.append(QColor(115, 219, 59));
    result.append(QColor(219, 59, 169));
    result.append(QColor(219, 163, 59));
    return result;
}

