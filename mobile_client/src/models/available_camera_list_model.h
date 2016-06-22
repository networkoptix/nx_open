#pragma once

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnAvailableCameraListModelPrivate;
class QnAvailableCameraListModel : public Connective<QAbstractListModel>
{
    Q_OBJECT

    typedef Connective<QAbstractListModel> base_type;

public:
    QnAvailableCameraListModel(QObject* parent = nullptr);
    ~QnAvailableCameraListModel();

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void refreshResource(const QnResourcePtr& resource, int role = -1);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

protected:
    virtual bool filterAcceptsResource(const QnResourcePtr& resource) const;

protected:
    void resetResourcesInternal();

private:
    QScopedPointer<QnAvailableCameraListModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnAvailableCameraListModel)
};

