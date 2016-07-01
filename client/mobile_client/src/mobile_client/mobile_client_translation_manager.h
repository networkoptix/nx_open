#pragma once

#include <translation/translation_manager.h>

class QnMobileClientTranslationManager : public QnTranslationManager
{
    Q_OBJECT

    typedef QnTranslationManager base_type;

public:
    explicit QnMobileClientTranslationManager(QObject *parent = nullptr);
    virtual ~QnMobileClientTranslationManager() override;

    void updateTranslation();
};
