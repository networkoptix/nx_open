#ifndef QN_DESKTOP_RESOURCE_H
#define QN_DESKTOP_RESOURCE_H

#include "core/resource/resource.h"
#include "plugins/resources/archive/abstract_archive_resource.h"

class QnDesktopResource: public QnAbstractArchiveResource {
    Q_OBJECT;
public:
    QnDesktopResource(int index, QGLWidget* mainWindow);

    virtual QString toString() const override;
    bool isRendererSlow() const;
protected:
    virtual QnAbstractStreamDataProvider *createDataProviderInternal(ConnectionRole role) override;
private:
    QGLWidget* m_mainWidget;
};

#endif // QN_DESKTOP_RESOURCE_H
