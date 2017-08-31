#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/radass/radass_fwd.h>

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

    RadassMode mode(const QnLayoutItemIndexList& items) const;
    void setMode(const QnLayoutItemIndexList& items, RadassMode value);

signals:
    void modeChanged(const QnLayoutItemIndex& item, RadassMode value);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
