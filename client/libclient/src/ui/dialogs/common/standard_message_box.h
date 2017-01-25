#pragma once

#include <QtCore/QObject>

class QString;
class QWidget;

class QnStandardMessageBox : public QObject
{
public:
    static bool overwriteFileQuestion(
        QWidget* parent,
        const QString& fileName);

    static void failedToOverwriteMessage(
        QWidget* parent,
        const QString& fileName);
};