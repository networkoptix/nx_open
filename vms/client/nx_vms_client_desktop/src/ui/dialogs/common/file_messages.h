// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
