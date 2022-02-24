// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <QtCore/QObject>
#include <QtGui/QFont>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

class QWidget;

namespace nx::vms::client::desktop {

class SceneBanners:
    public QObject,
    public Singleton<SceneBanners>
{
    Q_OBJECT

public:
    SceneBanners(QWidget* parentWidget);
    virtual ~SceneBanners() override;

    QnUuid add(const QString& text,
        std::optional<std::chrono::milliseconds> timeout = {},
        std::optional<QFont> font = {});

    bool remove(const QnUuid& id, bool immediately = false);

    bool changeText(const QnUuid& id, const QString& newText);

signals:
    void removed(const QnUuid& id);

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
    explicit SceneBanner(const QnUuid& id, QObject* parent = nullptr);

private:
    QnUuid m_id;
};

} // namespace nx::vms::client::desktop
