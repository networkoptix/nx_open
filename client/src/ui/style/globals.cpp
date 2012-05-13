#include "globals.h"

#include <cassert>

#include <QtCore/QSettings>
#include <QtGui/QFont>
#include <QtGui/QColor>

#include "config.h"

Q_GLOBAL_STATIC(QnGlobals, globalsInstance);

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
    QObject(parent)
{
    /* Ensure that default skin resource is loaded. 
     * This is needed because globals instance may be constructed before the
     * corresponding resource initializer is called. */
    Q_INIT_RESOURCE(skin);

    QScopedPointer<QSettings> settings(new QSettings(QString(QN_SKIN_PATH) + QLatin1String("/skin/globals.ini"), QSettings::IniFormat));

    settings->beginGroup(QLatin1String("globals"));
    init(Dummy<VARIABLE_COUNT>(), settings.data());
    settings->endGroup();
}

QnGlobals::~QnGlobals() {
    return;
}

void QnGlobals::initValue(Variable variable, QSettings *settings, const QString &key, const QColor &defaultValue) {
    setValue(variable, parseColor(settings->value(key), defaultValue));
}

void QnGlobals::initValue(Variable variable, QSettings *settings, const QString &key, int defaultValue) {
    bool ok = false;
    int value = settings->value(key).toInt(&ok);
    if(!ok)
        value = defaultValue;
    setValue(variable, value);
}

void QnGlobals::initValue(Variable variable, QSettings *settings, const QString &key, qreal defaultValue) {
    bool ok = false;
    qreal value = settings->value(key).toReal(&ok);
    if(!ok)
        value = defaultValue;
    setValue(variable, value);
}

QVariant QnGlobals::value(Variable variable) {
    assert(variable >= 0 && variable < VARIABLE_COUNT);

    return m_globals[variable];
}

void QnGlobals::setValue(Variable variable, const QVariant &value) {
    assert(variable >= 0 && variable < VARIABLE_COUNT);

    m_globals[variable] = value;
}

QnGlobals *QnGlobals::instance() {
    return globalsInstance();
}
