#pragma once

class QnClientSettingsWatcher: public QObject
{
    using base_type = QObject;
public:
    QnClientSettingsWatcher(QObject* parent = nullptr);
};
