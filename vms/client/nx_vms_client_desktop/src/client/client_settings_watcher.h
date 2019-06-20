#pragma once

#include <QtCore/QObject>

class QnClientSettingsWatcher: public QObject
{
    using base_type = QObject;

    Q_OBJECT
public:
    QnClientSettingsWatcher(QObject* parent = nullptr);
};
