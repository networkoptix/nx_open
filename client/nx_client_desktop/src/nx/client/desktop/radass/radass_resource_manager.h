#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/radass/radass_fwd.h>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

/**
 * Manages radass state for resources (layout items), saves it locally. Connection entries are
 * saved by system id to make sure we will clean all non-existent layout items sometimes.
 */
class RadassResourceManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    RadassResourceManager(QObject* parent = nullptr);
    virtual ~RadassResourceManager() override;

    RadassMode mode(const QnLayoutResourcePtr& layout) const;
    void setMode(const QnLayoutResourcePtr& layout, RadassMode value);

    RadassMode mode(const QnLayoutItemIndex& item) const;
    void setMode(const QnLayoutItemIndex& item, RadassMode value);

    RadassMode mode(const QnLayoutItemIndexList& items) const;
    void setMode(const QnLayoutItemIndexList& items, RadassMode value);


    QString cacheDirectory() const;
    void setCacheDirectory(const QString& value);

    /** Load stored settings for the given system, drop other. */
    void switchLocalSystemId(const QnUuid& localSystemId);

    /** Save items for the local system, cleanup non-existing. */
    void saveData(const QnUuid& localSystemId, QnResourcePool* resourcePool) const;

signals:
    void modeChanged(const QnLayoutItemIndex& item, RadassMode value);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
