#pragma once

#include <QtCore/QObject>
#include <QtGui/QCursor>

#include <nx/utils/singleton.h>

class QnSkin;

namespace nx::vms::client::desktop {

class CustomCursors:
    public QObject,
    public Singleton<CustomCursors>
{
    Q_OBJECT
    Q_PROPERTY(QCursor sizeAll MEMBER sizeAll CONSTANT)
    Q_PROPERTY(QCursor cross MEMBER cross CONSTANT)

public:
    static const QCursor& sizeAll;
    static const QCursor& cross;

public:
    CustomCursors(QnSkin* skin);

    static void registerQmlType();

private:
    struct Private;
    static Private& staticData();
};

} // namespace nx::vms::client::desktop
