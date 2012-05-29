#ifndef QN_PROPERTY_STORAGE_H
#define QN_PROPERTY_STORAGE_H

#include <QtCore/QObject>
#include <QtCore/QVariant>

class QSettings;

class QnPropertyStorage: public QObject {
    Q_OBJECT;
public:
    QnPropertyStorage(QObject *parent = NULL);
    virtual ~QnPropertyStorage();

    QVariant value(int id) const;
    virtual void setValue(int id, const QVariant &value);

    QString name(int id) const;
    int type(int id) const;

    virtual void updateFromSettings(QSettings *settings);
    virtual void submitToSettings(QSettings *settings) const;

signals:
    void valueChanged(int id);

protected:
    virtual void updateFromSettings(QSettings *settings, int id);
    virtual void submitToSettings(QSettings *settings, int id) const;

    virtual void startRead(int id) const { Q_UNUSED(id); }
    virtual void endRead(int id) const { Q_UNUSED(id); }
    virtual void startWrite(int id) { Q_UNUSED(id); }
    virtual void endWrite(int id) { Q_UNUSED(id); }

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

#define QN_DECLARE_PROPERTY(TYPE, ACCESSOR, ID, DEFAULT_VALUE)                  \
public:                                                                         \
    inline TYPE ACCESSOR() { return value(ID).value<TYPE>(); }                  \
    inline void ACCESSOR(const TYPE &value) { setValue(ID, QVariant::fromValue<TYPE>(value)); } \
private:                                                                        \
    inline void init(const Dummy<ID> &) {                                       \
        init(Dummy<ID - 1>());                                                  \
        setType(ID, qMetaTypeId<TYPE>());                                       \
        setValue(ID, QVariant::fromValue<TYPE>(DEFAULT_VALUE));                 \
        setName(ID, QLatin1String(#ACCESSOR));                                  \
    }                                                                           \

#define QN_END_PROPERTY_STORAGE()                                               \

private:
    QHash<int, QVariant> m_valueById;
    QHash<int, QString> m_nameById;
    QHash<int, int> m_typeById;
};

#endif // QN_PROPERTY_STORAGE_H
