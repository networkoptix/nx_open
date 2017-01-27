#pragma once

#include <QObject>

class QString;
class QWidget;

class QnPtzMessages : public QObject
{
    Q_OBJECT
public:
    static bool confirmDeleteUsedPresed(QWidget* parent);

    static void failedToGetPosition(
        QWidget* parent,
        const QString& cameraName);

    static void failedToSetPosition(
        QWidget* parent,
        const QString& cameraName);

};