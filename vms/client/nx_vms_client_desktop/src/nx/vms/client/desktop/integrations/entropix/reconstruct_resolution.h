#pragma once

#include <QtCore/QRegularExpression>

#include "../interfaces.h"

class QnWorkbenchContext;

namespace nx::vms::client::desktop::integrations::entropix {

/**
 * Reconstruct Resolution feature adds a special context menu item on a timeline, which is allowed
 * only for cameras, which model is listed in the ini-config, and only if active widget is the zoom
 * window. This menu item effectively adds a bookmark with this zoom window coordinates.
 */
class ReconstructResolutionIntegration:
    public Integration,
    public IServerApiProcessor,
    public IMenuActionFactory
{
    Q_OBJECT
    using base_type = Integration;

public:
    explicit ReconstructResolutionIntegration(const QRegularExpression& models, QObject* parent);

    virtual void connectionEstablished(ec2::AbstractECConnectionPtr connection) override;

    virtual void registerActions(ui::action::MenuFactory* factory) override;

private:
    void addReconstructResolutionBookmark(QnWorkbenchContext* context);

private:
    QRegularExpression m_models;
};

} // namespace nx::vms::client::desktop::integrations::entropix
