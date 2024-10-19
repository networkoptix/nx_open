// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/url.h>

class QnContext;
class QnMobileClientUriHandler : public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit QnMobileClientUriHandler(QnContext* context, QObject* parent = nullptr);
    virtual ~QnMobileClientUriHandler() override;

    static QStringList supportedSchemes();
    static const char* handlerMethodName();

public slots:
    void handleUrl(const QUrl& url);

private:
    class Private;
    const QScopedPointer<Private> d;
};
