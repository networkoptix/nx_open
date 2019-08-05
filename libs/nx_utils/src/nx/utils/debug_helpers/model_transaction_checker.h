#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

class QAbstractItemModel;

namespace nx::utils {

class NX_UTILS_API ModelTransactionChecker: public QObject
{
public:
    ModelTransactionChecker() = delete;
    virtual ~ModelTransactionChecker() override;

    static ModelTransactionChecker* install(QAbstractItemModel* model);

private:
    ModelTransactionChecker(QAbstractItemModel* parent);

private:
    class Private;
    ImplPtr<Private> d;
};

} // namespace nx::utils
