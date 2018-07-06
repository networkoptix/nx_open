#pragma once

#include <QtCore/QObject>

#include <nx/client/core/ui/frame_section.h>

namespace nx {
namespace client {
namespace desktop {

class CursorManager: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* target READ target WRITE setTarget NOTIFY targetChanged)

public:
    CursorManager(QObject* parent = nullptr);
    virtual ~CursorManager() override;

    QObject* target() const;
    void setTarget(QObject* target);

    Q_INVOKABLE void setCursor(QObject* requester, Qt::CursorShape shape, int rotation);
    // TODO: Use FrameSection::Section once QTBUG-58454 is fixed.
    Q_INVOKABLE void setCursorForFrameSection(
        QObject* requester, int section, int rotation = 0);
    Q_INVOKABLE void setCursorForFrameSection(
        int section, int rotation = 0);
    Q_INVOKABLE void unsetCursor(QObject* requester = nullptr);

    static void registerQmlType();

signals:
    void targetChanged();

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace desktop
} // namespace client
} // namespace nx
