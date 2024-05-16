// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QIdentityProxyModel>
#include <QtQuick/QQuickImageProvider>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

/**
 * Decoration model that watches on visible items and manages preview for them.
 * Ownership of the source model should be managed outside.
 */
class NX_VMS_CLIENT_CORE_API VisibleItemDataDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
    class PreviewProvider;

    struct Settings
    {
        // Load previews no more than one per this time period.
        std::chrono::milliseconds tilePreviewLoadIntervalMs = std::chrono::milliseconds(0);

        // A delay after which preview may be requested again after receiving "NO DATA".
        std::chrono::milliseconds previewReloadDelayMs = std::chrono::milliseconds(30);

        int maxSimultaneousPreviewLoadsArm = 3;
    };

    VisibleItemDataDecoratorModel(const Settings& settings, QObject* parent = nullptr);
    virtual ~VisibleItemDataDecoratorModel() override;

    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual void setSourceModel(QAbstractItemModel* value) override;

    bool previewsEnabled() const;
    void setPreviewsEnabled(bool value);

    bool isVisible(const QModelIndex& index) const;

signals:
    void previewsEnabledChanged();

private:
    using base_type::setSourceModel;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class NX_VMS_CLIENT_CORE_API VisibleItemDataDecoratorModel::PreviewProvider:
    public QQuickImageProvider
{
public:
    static constexpr auto id = QLatin1String("previews");

    PreviewProvider();

    virtual QImage requestImage(const QString& id,
        QSize* size,
        const QSize& /*requestedSize*/) override;
};

} // namespace nx::vms::client::core
