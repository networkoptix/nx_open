// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <QtCore/QObject>
#include <QtGui/QFont>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

class QWidget;

namespace nx::vms::client::desktop {

class SceneBanners: public QObject
{
    Q_OBJECT

public:
    SceneBanners(QWidget* parentWidget);
    virtual ~SceneBanners() override;

signals:
    void removed(nx::Uuid id);

private:
    friend class SceneBanner;

    static SceneBanners* instance();

    nx::Uuid add(const QString& text,
        std::optional<std::chrono::milliseconds> timeout = {},
        std::optional<QFont> font = {});

    bool remove(const nx::Uuid& id, bool immediately = false);

    bool changeText(const nx::Uuid& id, const QString& newText);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

class SceneBanner: public QObject
{
    Q_OBJECT

public:
    static SceneBanner* show(const QString& text,
        std::optional<std::chrono::milliseconds> timeout = {},
        std::optional<QFont> font = {});

    virtual ~SceneBanner() override;

    bool hide(bool immediately = false);

    bool changeText(const QString& value);

protected:
    SceneBanner(const nx::Uuid& id, QObject* parent);

    SceneBanners* banners() const;

private:
    nx::Uuid m_id;
};

} // namespace nx::vms::client::desktop
