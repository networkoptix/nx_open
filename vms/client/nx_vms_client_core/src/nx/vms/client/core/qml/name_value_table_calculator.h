// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQml/QQmlParserStatus>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/context_from_qml_handler.h>

class QQuickItem;

namespace nx::vms::client::core {

class NameValueTableCalculator: public QObject, public ContextFromQmlHandler
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    using base_type = QObject;

public:
    explicit NameValueTableCalculator(QObject* parent = nullptr);
    virtual ~NameValueTableCalculator() override;

    Q_INVOKABLE void updateColumnWidths();

    static void registerQmlType();

private:
    virtual void onContextReady() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
