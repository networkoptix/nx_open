#ifndef QN_PROPERTY_STORAGE_H
#define QN_PROPERTY_STORAGE_H

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtCore/QStringList>

class QSettings;
class QnMutex;
class QTextStream;

class QnPropertyStorageLocker;

class QnPropertyNotifier: public QObject {
    Q_OBJECT;
public:
    QnPropertyNotifier(QObject *parent = NULL): QObject(parent) {}

signals:
    void valueChanged(int id);

private:
    friend class QnPropertyStorage;
};


/**
 * Storage of typed key-value pairs that supports value change notifications,
 * writing and reading to/from <tt>QSettings</tt> and updating from command line.
 * 
 * The typical usage is to derive from this class, create an enumeration
 * for ids of all properties, and then declare them using 
 * <tt>QN_DECLARE_R_PROPERTY</tt> and <tt>QN_DECLARE_RW_PROPERTY</tt> macros,
 * wrapped in a pair of <tt>QN_BEGIN_PROPERTY_STORAGE</tt> and 
 * <tt>QN_END_PROPERTY_STORAGE</tt> invocations. These macros will generate
 * an <tt>init()</tt> function, that is then to be called from derived class's
 * constructor.
 */
class QnPropertyStorage: public QObject {
    Q_OBJECT;
public:
    QnPropertyStorage(QObject *parent = NULL);
    virtual ~QnPropertyStorage();

    QList<int> variables() const;

    QVariant value(int id) const;
    bool setValue(int id, const QVariant &value);

    QVariant value(const QString &name) const;
    bool setValue(const QString &name, const QVariant &value);

    QnPropertyNotifier *notifier(int id) const;

    QString name(int id) const;
    void setName(int id, const QString &name);

    void addArgumentName(int id, const char *argumentName);
    void addArgumentName(int id, const QString &argumentName);
    void setArgumentNames(int id, const QStringList &argumentNames);
    QStringList argumentNames(int id) const;

    int type(int id) const;
    void setType(int id, int type);

    bool isWritable(int id) const;
    void setWritable(int id, bool writable);

    bool isThreadSafe() const;
    void setThreadSafe(bool threadSafe);

    // TODO: #Elric we need a 'dirty' flag and several submit modes. Default mode is to write out only those settings that were actually changed.

    void updateFromSettings(QSettings *settings);
    void submitToSettings(QSettings *settings) const;

    void updateFromJson(const QJsonObject &json);

    // TODO: #Elric we need a way to make command line parameters not to be saved to settings if they are not changed.

    bool updateFromCommandLine(int &argc, char **argv, FILE *errorFile);
    bool updateFromCommandLine(int &argc, char **argv, QTextStream *errorStream);

signals:
    void valueChanged(int id);

protected:
    enum UpdateStatus {
        Changed,    /**< Value was changed. */
        Skipped,    /**< Value didn't change. */
        Failed,     /**< An error has occurred. */
    };

    virtual void updateValuesFromSettings(QSettings *settings, const QList<int> &ids);
    virtual void submitValuesToSettings(QSettings *settings, const QList<int> &ids) const;
    virtual QVariant readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue);
    virtual void writeValueToSettings(QSettings *settings, int id, const QVariant &value) const;

    virtual void updateValuesFromJson(const QJsonObject &json, const QList<int> &ids);
    virtual QVariant readValueFromJson(const QJsonObject &json, int id, const QVariant &defaultValue);

    virtual UpdateStatus updateValue(int id, const QVariant &value);

#define QN_BEGIN_PROPERTY_STORAGE(LAST_ID)                                      \
private:                                                                        \
    template<int> struct Dummy {};                                              \
                                                                                \
    template<int id>                                                            \
    inline void init(const Dummy<id> &) { init(Dummy<id - 1>()); }              \
    inline void init(const Dummy<-1> &) {}                                      \
                                                                                \
    inline void init() { init(Dummy<LAST_ID>()); };                             \

#define QN_DECLARE_PROPERTY(TYPE, ID, NAME, WRITABLE, DEFAULT_VALUE)            \
private:                                                                        \
    inline void init(const Dummy<ID> &) {                                       \
        init(Dummy<ID - 1>());                                                  \
        setType(ID, qMetaTypeId<TYPE>());                                       \
        setName(ID, QLatin1String(NAME));                                       \
        setValue(ID, QVariant::fromValue<TYPE>(DEFAULT_VALUE));                 \
        setWritable(ID, WRITABLE);                                              \
    }                                                                           \

#define QN_DECLARE_R_PROPERTY(TYPE, GETTER, ID, DEFAULT_VALUE)                  \
    QN_DECLARE_PROPERTY(TYPE, ID, #GETTER, false, DEFAULT_VALUE)                \
public:                                                                         \
    inline TYPE GETTER() const { return value(ID).value<TYPE>(); }              \
private:                                                                        \

#define QN_DECLARE_RW_PROPERTY(TYPE, GETTER, SETTER, ID, DEFAULT_VALUE)         \
    QN_DECLARE_PROPERTY(TYPE, ID, #GETTER, true, DEFAULT_VALUE)                 \
public:                                                                         \
    inline TYPE GETTER() const { return value(ID).value<TYPE>(); }              \
    inline void SETTER(const TYPE &value) { setValue(ID, QVariant::fromValue<TYPE>(value)); } \
private:                                                                        \

#define QN_END_PROPERTY_STORAGE()                                               \

private:
    friend class QnPropertyStorageLocker;

    void lock() const;
    void unlock() const;
    void notify(int id) const;

    bool isWritableLocked(int id) const;
    QVariant valueLocked(int id) const;
    bool setValueLocked(int id, const QVariant &value);

private:
    bool m_threadSafe;
    QScopedPointer<QnMutex> m_mutex;
    mutable int m_lockDepth;
    mutable QSet<int> m_pendingNotifications;

    QHash<int, QVariant> m_valueById;
    QHash<int, QString> m_nameById;
    QHash<QString, int> m_idByName;
    QHash<int, QStringList> m_argumentNamesById;
    QHash<int, int> m_typeById;
    QHash<int, bool> m_writableById;
    mutable QHash<int, QnPropertyNotifier *> m_notifiers;
};

#endif // QN_PROPERTY_STORAGE_H
