#pragma once

#include <QtCore/QIdentityProxyModel>
#include <QtQuick/QQuickImageProvider>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>

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
    virtual ~RightPanelModelsAdapter() override;

    QnWorkbenchContext* context() const;
    void setContext(QnWorkbenchContext* value);

    RightPanel::Tab type() const;
    void setType(RightPanel::Tab value);

    Q_INVOKABLE bool removeRow(int row);

    Q_INVOKABLE void setAutoClosePaused(int row, bool value);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setFetchDirection(nx::vms::client::desktop::RightPanel::FetchDirection value);
    Q_INVOKABLE void requestFetch();

    Q_INVOKABLE void setLivePaused(bool value);

    static void registerQmlType();

signals:
    void contextChanged();
    void typeChanged();
    void dataNeeded();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

class RightPanelImageProvider: public QQuickImageProvider
{
public:
    RightPanelImageProvider(): QQuickImageProvider(Image) {}
    virtual QImage requestImage(const QString& id, QSize* size, const QSize& /*requestedSize*/);
};

} // namespace nx::vms::client::desktop
