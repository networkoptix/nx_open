#pragma once

#include <QtCore/QObject>

class QnCheckbox: public QObject {
    Q_OBJECT
public:
    explicit QnCheckbox(QObject* parent = NULL);
    ~QnCheckbox();

    static void autoCleanTristate(QCheckBox* checkbox);

private:
    void cleanTristate();
};
