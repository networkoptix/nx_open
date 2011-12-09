#ifndef EVE_APP_H_
#define EVE_APP_H_

#include <QtSingleApplication>

class EveApplication : public QtSingleApplication
{
    Q_OBJECT

public:
    EveApplication(int &argc, char **argv);

    QStringList getFileNames() const;
    bool event(QEvent *event);

private:
    QStringList m_fileNames;
};

#endif // EVE_APP_H_
