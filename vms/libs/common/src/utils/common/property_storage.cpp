#include "property_storage.h"

#include <cassert>

#include <QtCore/QMetaType>
#include <QtCore/QSettings>
#include <nx/utils/thread/mutex.h>
#include <QtGui/QColor>

#include <nx/fusion/serialization/json_functions.h>

#include "warnings.h"
#include "command_line_parser.h"

// -------------------------------------------------------------------------- //
// QnPropertyStorageLocker
// -------------------------------------------------------------------------- //
class QnPropertyStorageLocker {
public:
    QnPropertyStorageLocker(const QnPropertyStorage *storage): m_storage(storage) {
        m_storage->lock();
    }

    ~QnPropertyStorageLocker() {
        m_storage->unlock();
    }

private:
    const QnPropertyStorage *m_storage;
};


// -------------------------------------------------------------------------- //
// QnPropertyStorage
// -------------------------------------------------------------------------- //
QnPropertyStorage::QnPropertyStorage(QObject *parent):
    QObject(parent),
    m_threadSafe(false),
    m_lockDepth(0)
{}

QnPropertyStorage::~QnPropertyStorage() {
    return;
}

QList<int> QnPropertyStorage::variables() const {
    return m_valueById.keys();
}

QVariant QnPropertyStorage::valueLocked(int id) const {
    return m_valueById.value(id);
}

bool QnPropertyStorage::setValueLocked(int id, const QVariant &value) {
    if(!isWritableLocked(id)) {
        qnWarning("Property '%1' is not writeable.", name(id));
        return false;
    }

    return updateValue(id, value) != Failed;
}

QVariant QnPropertyStorage::value(int id) const {
    QnPropertyStorageLocker locker(this);
    return valueLocked(id);
}

bool QnPropertyStorage::setValue(int id, const QVariant &value) {
    QnPropertyStorageLocker locker(this);
    return setValueLocked(id, value);
}

QVariant QnPropertyStorage::value(const QString &name) const {
    QnPropertyStorageLocker locker(this);

    auto pos = m_idByName.find(name);
    if(pos == m_idByName.end())
        return QVariant();

    return valueLocked(*pos);
}

bool QnPropertyStorage::setValue(const QString &name, const QVariant &value) {
    QnPropertyStorageLocker locker(this);

    auto pos = m_idByName.find(name);
    if(pos == m_idByName.end())
        return false;

    return setValueLocked(*pos, value);
}

QnPropertyStorage::UpdateStatus QnPropertyStorage::updateValue(int id, const QVariant &value) {
    QVariant newValue = value;

    int type = m_typeById.value(id, QMetaType::UnknownType);
    if(type != QMetaType::UnknownType && int(value.type()) != type) {
        if(!newValue.convert(static_cast<QVariant::Type>(type))) {
            qnWarning("Cannot assign a value of type '%1' to a property '%2' of type '%3'.", QMetaType::typeName(value.userType()), name(id), QMetaType::typeName(type));
            return Failed;
        }
    }

    if(m_valueById.value(id) == newValue)
        return Skipped;
    m_valueById[id] = value;

    notify(id);

    return Changed;
}

QnPropertyNotifier *QnPropertyStorage::notifier(int id) const {
    QnPropertyStorageLocker locker(this);

    QnPropertyNotifier *&result = m_notifiers[id];
    if(!result)
        result = new QnPropertyNotifier(const_cast<QnPropertyStorage *>(this));
    return result;
}

QString QnPropertyStorage::name(int id) const {
    QnPropertyStorageLocker locker(this);

    return m_nameById.value(id);
}

void QnPropertyStorage::setName(int id, const QString &name) {
    QnPropertyStorageLocker locker(this);

    m_nameById[id] = name;
    m_idByName[name] = id;
}

void QnPropertyStorage::addArgumentName(int id, const char *argumentName) {
    addArgumentName(id, QLatin1String(argumentName));
}

void QnPropertyStorage::addArgumentName(int id, const QString &argumentName) {
    QnPropertyStorageLocker locker(this);

    m_argumentNamesById[id].push_back(argumentName);
}

void QnPropertyStorage::setArgumentNames(int id, const QStringList &argumentNames) {
    QnPropertyStorageLocker locker(this);

    m_argumentNamesById[id] = argumentNames;
}

QStringList QnPropertyStorage::argumentNames(int id) const {
    QnPropertyStorageLocker locker(this);

    return m_argumentNamesById.value(id);
}

int QnPropertyStorage::type(int id) const {
    QnPropertyStorageLocker locker(this);

    return m_typeById.value(id, QMetaType::UnknownType);
}

void QnPropertyStorage::setType(int id, int type) {
    QnPropertyStorageLocker locker(this);

    if(m_typeById[id] == type)
        return;

    m_typeById[id] = type;

    if(type != QMetaType::UnknownType)
        updateValue(id, QVariant(type, static_cast<const void *>(NULL)));
}

bool QnPropertyStorage::isWritableLocked(int id) const {
    return m_writableById.value(id, true); /* By default, properties are writable. */
}

bool QnPropertyStorage::isWritable(int id) const {
    QnPropertyStorageLocker locker(this);

    return isWritableLocked(id);
}

void QnPropertyStorage::setWritable(int id, bool writable) {
    QnPropertyStorageLocker locker(this);

    m_writableById[id] = writable;
}

bool QnPropertyStorage::isThreadSafe() const {
    return m_threadSafe;
}

void QnPropertyStorage::setThreadSafe(bool threadSafe) {
    m_threadSafe = threadSafe;

    if(m_threadSafe && !m_mutex)
        m_mutex.reset(new QnMutex(QnMutex::Recursive));
}

void QnPropertyStorage::updateFromSettings(QSettings *settings) {
    if(!settings) {
        qnNullWarning(settings);
        return;
    }

    QnPropertyStorageLocker locker(this);
    updateValuesFromSettings(settings, m_nameById.keys());
}

void QnPropertyStorage::updateFromJson(const QJsonObject &json) {
    QnPropertyStorageLocker locker(this);
    updateValuesFromJson(json, m_nameById.keys());
}

void QnPropertyStorage::submitToSettings(QSettings *settings) const {
    if(!settings) {
        qnNullWarning(settings);
        return;
    }

    QnPropertyStorageLocker locker(this);
    submitValuesToSettings(settings, m_nameById.keys());
}

bool QnPropertyStorage::updateFromCommandLine(int &argc, const char **argv, FILE *errorFile) {
    if(errorFile) {
        QTextStream errorStream(errorFile);
        return updateFromCommandLine(argc, argv, &errorStream);
    } else {
        return updateFromCommandLine(argc, argv, static_cast<QTextStream *>(NULL));
    }
}

bool QnPropertyStorage::updateFromCommandLine(int &argc, const char **argv, QTextStream *errorStream) {
    QnPropertyStorageLocker locker(this);

    QList<int> ids = m_argumentNamesById.keys();

    QnCommandLineParser parser;
    for(int id: ids) {
        if(!isWritableLocked(id)) {
            qnWarning("Argument name is set for non-writable property '%1', property will not be read.", m_nameById.value(id));
            continue;
        }

        int type = m_typeById.value(id, QMetaType::UnknownType);

        for(const QString &name: m_argumentNamesById.value(id)) {
            parser.addParameter(
                QMetaType::QString, /* We don't want the parser to perform type checks. */
                name,
                QString(),
                QString(),
                type == QMetaType::Bool ? QVariant(QLatin1String("true")) : QVariant()
            );
        }
    }

    if(!parser.parse(argc, argv, errorStream))
        return false;

    bool result = true;
    for(int id: ids) {
        QStringList names = m_argumentNamesById.value(id);
        if(names.isEmpty())
            continue;
        QString name = names.front();

        QVariant value = parser.value(name);
        if(!value.isValid())
            continue;

        UpdateStatus status = updateValue(id, value);
        if(status == Failed) {
            if(errorStream) {
                QString message = lit("Invalid value for '%1' argument - expected %2, provided '%3'.").
                    arg(name).
                    arg(QLatin1String(QMetaType::typeName(m_typeById.value(id, QMetaType::UnknownType)))).
                    arg(value.toString());

                *errorStream << message << endl;
            }
            result = false;
        }
    }

    return result;
}

void QnPropertyStorage::lock() const {
    if(m_threadSafe)
        m_mutex->lock();
    m_lockDepth++;
}

void QnPropertyStorage::unlock() const {
    m_lockDepth--;

    if(m_lockDepth == 0 && !m_pendingNotifications.isEmpty()) {
        QSet<int> pendingNotifications = m_pendingNotifications;
        QHash<int, QnPropertyNotifier *> notifiers = m_notifiers;
        m_pendingNotifications.clear();

        if(m_threadSafe)
            m_mutex->unlock();

        for(int id: pendingNotifications) {
            QnPropertyNotifier *notifier = notifiers.value(id);

            emit const_cast<QnPropertyStorage *>(this)->valueChanged(id);
            if(notifier)
                emit notifier->valueChanged(id);
        }
    } else {
        if(m_threadSafe)
            m_mutex->unlock();
    }
}

void QnPropertyStorage::notify(int id) const {
    NX_ASSERT(m_lockDepth > 0);

    m_pendingNotifications.insert(id);
}

void QnPropertyStorage::updateValuesFromSettings(QSettings *settings, const QList<int> &ids) {
    for(int id: ids)
        updateValue(id, readValueFromSettings(settings, id, value(id)));
}

void QnPropertyStorage::submitValuesToSettings(QSettings *settings, const QList<int> &ids) const {
    for(int id: ids)
        if(isWritableLocked(id))
            writeValueToSettings(settings, id, value(id));
}

QVariant QnPropertyStorage::readValueFromSettings(QSettings *settings, int id,
    const QVariant &defaultValue) const
{
    /* In some cases QSettings::value returns invalid QVariant even when supplied
     * an adequate default value. So we check it manually. */
    QVariant value = settings->value(name(id));
    return value.isValid() ? value : defaultValue;
}

void QnPropertyStorage::writeValueToSettings(QSettings *settings, int id, const QVariant &value) const {
    settings->setValue(name(id), value);
}

void QnPropertyStorage::updateValuesFromJson(const QJsonObject &json, const QList<int> &ids) {
    for(int id: ids)
        updateValue(id, readValueFromJson(json, id, value(id)));
}

QVariant QnPropertyStorage::readValueFromJson(const QJsonObject &json, int id, const QVariant &defaultValue) {
    QJsonValue jsonValue;
    if(!QJson::deserialize(json, name(id), &jsonValue))
        return defaultValue;

    int type = this->type(id);
    QnJsonSerializer *serializer = QnJsonSerializer::serializer(type);
    if(!serializer) {
        qnWarning("Could not deserialize type '%1' from json, serializer is not registered.", QMetaType::typeName(type));
        return defaultValue;
    }

    QVariant result;
    QnJsonContext ctx;
    if(!serializer->deserialize(&ctx, jsonValue, &result)) {
        qnWarning("Invalid json value '%1' for type '%2'.", jsonValue, QMetaType::typeName(type));
        return defaultValue;
    }

    return result;
}
