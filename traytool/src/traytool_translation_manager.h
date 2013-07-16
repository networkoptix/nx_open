#ifndef QN_TRAYTOOL_TRANSLATION_MANAGER_H
#define QN_TRAYTOOL_TRANSLATION_MANAGER_H

#include <translation/translation_manager.h>

class QnTraytoolTranslationManager: public QnTranslationManager {
    Q_OBJECT
    typedef QnTranslationManager base_type;

public:
    QnTraytoolTranslationManager(QObject *parent = NULL): 
        base_type(parent)
    {
        QList<QString> prefixes;
        prefixes.push_back(lit("traytool"));
        prefixes.push_back(lit("common"));
        prefixes.push_back(lit("qt"));

        QList<QString> searchPaths;
        searchPaths.push_back(QLatin1String(":/translations"));
        searchPaths.push_back(QApplication::applicationDirPath() + QLatin1String("/translations"));

        setPrefixes(prefixes);
        setSearchPaths(searchPaths);
    }
};

#endif // QN_TRAYTOOL_TRANSLATION_MANAGER_H
