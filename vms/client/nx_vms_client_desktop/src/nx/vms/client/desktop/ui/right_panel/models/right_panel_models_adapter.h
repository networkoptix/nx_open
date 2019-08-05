#pragma once

#include <QtCore/QIdentityProxyModel>

#include <nx/vms/client/desktop/ui/right_panel/right_panel_globals.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class RightPanelModelsAdapter: public QIdentityProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QnWorkbenchContext* context READ context WRITE setContext NOTIFY contextChanged)
    Q_PROPERTY(nx::vms::client::desktop::RightPanel::Tab type
        READ type WRITE setType NOTIFY typeChanged)

    using base_type = QIdentityProxyModel;

public:
    RightPanelModelsAdapter(QObject* parent = nullptr);
    virtual ~RightPanelModelsAdapter() override = default;

    QnWorkbenchContext* context() const;
    void setContext(QnWorkbenchContext* context);

    RightPanel::Tab type() const;
    void setType(RightPanel::Tab value);

    Q_INVOKABLE bool removeRow(int row);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    static void registerQmlType();

signals:
    void contextChanged();
    void typeChanged();

private:
    void recreateSourceModel();

private:
    QnWorkbenchContext* m_context = nullptr;
    RightPanel::Tab m_type = RightPanel::Tab::invalid;
};

} // namespace nx::vms::client::desktop
