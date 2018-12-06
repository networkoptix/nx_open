#include "resource.h"

#include <nx/fusion/serialization/json_functions.h>

template<typename Value>
class QnJsonResourceProperty
{
public:
    QnJsonResourceProperty(QnResourcePtr resource, QString name);

    struct Editor
    {
    public:
        Editor(QnJsonResourceProperty* parent);
        ~Editor();

        Editor(const Editor&) = delete;
        Editor& operator=(const Editor&) = delete;

        Editor(Editor&& editor);
        Editor& operator=(Editor&&);

        Value* operator->() { return &m_value; }
        Value& operator*() { return m_value; }

    private:
        QnJsonResourceProperty* m_parent = nullptr;
        Value m_value;
    };

    Value read() const;
    void write(const Value& value);
    Editor edit();

private:
    const QnResourcePtr m_resource;
    const QString m_name;
};

//-------------------------------------------------------------------------------------------------

template<typename Value>
QnJsonResourceProperty<Value>::QnJsonResourceProperty(QnResourcePtr resource, QString name):
    m_resource(std::move(resource)),
    m_name(std::move(name))
{
}

template<typename Value>
QnJsonResourceProperty<Value>::Editor::Editor(QnJsonResourceProperty* parent):
    m_parent(parent),
    m_value(m_parent->read())
{
}

template<typename Value>
QnJsonResourceProperty<Value>::Editor::Editor(Editor&& rhs):
    m_parent(rhs.m_parent),
    m_value(std::move(rhs.m_parent))
{
    m_parent = nullptr;
}

template<typename Value>
typename QnJsonResourceProperty<Value>::Editor& QnJsonResourceProperty<Value>::Editor::operator=(Editor&& rhs)
{
    std::swap(m_parent, rhs.m_parent);
    std::swap(m_value, rhs.value);
}

template<typename Value>
QnJsonResourceProperty<Value>::Editor::~Editor()
{
    m_parent->write(m_value);
}

template<typename Value>
Value QnJsonResourceProperty<Value>::read() const
{
    return QJson::deserialized<Value>(m_resource->getProperty(m_name).toUtf8());
}

template<typename Value>
void QnJsonResourceProperty<Value>::write(const Value& value)
{
    m_resource->setProperty(m_name, QString::fromUtf8(QJson::serialized(value)));
}

template<typename Value>
typename QnJsonResourceProperty<Value>::Editor QnJsonResourceProperty<Value>::edit()
{
    return Editor(this);
}
