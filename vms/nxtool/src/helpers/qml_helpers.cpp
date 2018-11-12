
#include "qml_helpers.h"

namespace 
{
    class FakeQmlParent : public QObject
    {
    public:
        virtual ~FakeQmlParent()
        {
            for (auto child : children())
            {
                child->setParent(nullptr);
            }
        }
    };
}

QObject *rtu::helpers::qml_objects_parent()
{
    static FakeQmlParent qmlObjectsParent;
    return &qmlObjectsParent;
}
