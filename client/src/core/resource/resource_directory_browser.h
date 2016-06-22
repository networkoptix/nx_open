#pragma once

#include "core/resource_management/resource_searcher.h"

class QnResourceDirectoryBrowser :
    public QObject,
    public QnAbstractFileResourceSearcher
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnResourceDirectoryBrowser(QObject* parent = nullptr);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacture() const override;
    virtual QnResourceList findResources() override;

    virtual QnResourcePtr checkFile(const QString &filename) const override;

    // Local files search only once. Use cleanup before search to re-search files again
    void cleanup();

    static QnLayoutResourcePtr layoutFromFile(const QString& xfile);
    static QnResourcePtr resourceFromFile(const QString &filename);

protected:
    bool m_resourceReady;

    static QnResourcePtr createArchiveResource(const QString& xfile);

    void findResources(const QString &directory, QnResourceList *result);
};

