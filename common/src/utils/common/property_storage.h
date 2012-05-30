#ifndef QN_PROPERTY_STORAGE_H
#define QN_PROPERTY_STORAGE_H

#include <QtCore/QObject>
#include <QtCore/QVariant>

class QSettings;

class QnPropertyNotifier: public QObject {
    Q_OBJECT;
public:
    QnPropertyNotifier(QObject *parent = NULL): QObject(parent) {}

signals:
    void valueChanged(int id);

private:
    friend class QnPropertyStorage;
};


class QnPropertyStorage: public QObject {
    Q_OBJECT;
public:
    QnPropertyStorage(QObject *parent = NULL);
    virtual ~QnPropertyStorage();

    QVariant value(int id) const;
    virtual bool setValue(int id, const QVariant &value);

    QnPropertyNotifier *notifier(int id) const;

    QString name(int id) const;
    int type(int id) const;

    virtual void updateFromSettings(QSettings *settings);
    virtual void submitToSettings(QSettings *settings) const;

signals:
    void valueChanged(int id);

protected:
    virtual QVariant updateValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue);
    virtual void submitValueToSettings(QSettings *settings, int id, const QVariant &value) const;

    virtual void lock() const {}
    virtual void unlock() const {}

    void setName(int id, const QString &name);
    void setType(int id, int type);

#define QN_BEGIN_PROPERTY_STORAGE(LAST_ID)                                      \
    template<int> struct Dummy {};                                              \
                                                                                \
    template<int id>                                                            \
    inline void init(const Dummy<id> &) { init(Dummy<id - 1>()); }              \
    inline void init(const Dummy<-1> &) {}                                      \
                                                                                \
    inline void init() { init(Dummy<LAST_ID>()); };                             \

#define QN_DECLARE_R_PROPERTY(TYPE, GETTER, ID, DEFAULT_VALUE)                  \
public:                                                                         \
    inline TYPE GETTER() const { return value(ID).value<TYPE>(); }              \
private:                                                                        \
    inline void init(const Dummy<ID> &) {                                       \
        init(Dummy<ID - 1>());                                                  \
        setType(ID, qMetaTypeId<TYPE>());                                       \
        setValue(ID, QVariant::fromValue<TYPE>(DEFAULT_VALUE));                 \
        setName(ID, QLatin1String(#GETTER));                                    \
    }                                                                           \

#define QN_DECLARE_RW_PROPERTY(TYPE, GETTER, SETTER, ID, DEFAULT_VALUE)         \
    QN_DECLARE_R_PROPERTY(TYPE, GETTER, ID, DEFAULT_VALUE)                      \
public:                                                                         \
    inline void SETTER(const TYPE &value) { setValue(ID, QVariant::fromValue<TYPE>(value)); } \
private:                                                                        \

#define QN_END_PROPERTY_STORAGE()                                               \

private:
    QHash<int, QVariant> m_valueById;
    QHash<int, QString> m_nameById;
    QHash<int, int> m_typeById;
    mutable QHash<int, QnPropertyNotifier *> m_notifiers;
};

#endif // QN_PROPERTY_STORAGE_H
