#pragma once

#include <QtCore/QObject>

class QString;
class QWidget;

class QnFileMessages : public QObject
{
    Q_OBJECT
public:
    static bool confirmOverwrite(
        QWidget* parent,
        const QString& fileName);

    static void overwriteFailed(
        QWidget* parent,
        const QString& fileName);
};