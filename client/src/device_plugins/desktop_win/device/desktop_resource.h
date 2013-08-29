#ifndef QN_DESKTOP_RESOURCE_H
#define QN_DESKTOP_RESOURCE_H

#include "core/resource/resource.h"
#include "plugins/resources/archive/abstract_archive_resource.h"
#include "../desktop_data_provider_wrapper.h"

class QnDesktopDataProvider;

class QnDesktopResource: public QnAbstractArchiveResource {
    Q_OBJECT;
public:
    QnDesktopResource(int index, QGLWidget* mainWindow);
    virtual ~QnDesktopResource();

    virtual QString toString() const override;
    bool isRendererSlow() const;
protected:
    virtual QnAbstractStreamDataProvider *createDataProviderInternal(ConnectionRole role) override;
private:
    void createSharedDataProvider();

    friend class QnDesktopDataProviderWrapper;

    void beforeStartDataProvider(QnDesktopDataProviderWrapper*);
    void beforeDestroyDataProvider(QnDesktopDataProviderWrapper* dataProviderWrapper);

private:
    QGLWidget* m_mainWidget;
    QnDesktopDataProvider* m_desktopDataProvider;
    QMutex m_dpMutex;
};

#endif // QN_DESKTOP_RESOURCE_H
