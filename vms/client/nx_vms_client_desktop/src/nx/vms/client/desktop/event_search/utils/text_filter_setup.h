// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace nx::vms::client::desktop {

class TextFilterSetup: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged FINAL)

public:
    explicit TextFilterSetup(QObject* parent = nullptr);

    QString text() const;
    void setText(const QString& value);

signals:
    void textChanged();

private:
    QString m_text;
};

} // namespace nx::vms::client::desktop
