#pragma once

#include <QtCore/QObject>

class QString;
class QWidget;

class QnCustomMessageBox : public QObject
{
public:
    static void showFailedToGetPosition(
        QWidget* parent,
        const QString& cameraName);

    static void showFailedToSetPosition(
        QWidget* parent,
        const QString& cameraName);

    static void showFailedRestartClient(QWidget* parent);

    static void showAnotherVideoWallExist(QWidget* parent);

};