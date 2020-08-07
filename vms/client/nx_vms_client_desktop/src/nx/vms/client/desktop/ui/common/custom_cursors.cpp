#include "custom_cursors.h"

#include <memory>

#include <QtQml/QtQml>

#include <ui/style/skin.h>

namespace nx::vms::client::desktop {

struct CustomCursors::Private
{
    QCursor sizeAllCursor;
    QCursor crossCursor;
};

const QCursor& CustomCursors::sizeAll = CustomCursors::staticData().sizeAllCursor;
const QCursor& CustomCursors::cross = CustomCursors::staticData().crossCursor;

CustomCursors::Private& CustomCursors::staticData()
{
    static Private data;
    return data;
}

CustomCursors::CustomCursors(QnSkin* skin)
{
    staticData().sizeAllCursor = QCursor{skin->pixmap("cursors/size_all.png")};
    staticData().crossCursor = QCursor{skin->pixmap("cursors/cross.png")};
}

void CustomCursors::registerQmlType()
{
    qmlRegisterSingletonType<CustomCursors>("nx.vms.client.desktop", 1, 0, "CustomCursors",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            qmlEngine->setObjectOwnership(instance(), QQmlEngine::CppOwnership);
            return instance();
        });
}

} // namespace nx::vms::client::desktop
