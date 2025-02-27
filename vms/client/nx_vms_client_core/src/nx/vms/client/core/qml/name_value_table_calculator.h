// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQml/QQmlParserStatus>

#include <nx/utils/impl_ptr.h>

class QQuickItem;

namespace nx::vms::client::core {

class NameValueTableCalculator: public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    using base_type = QObject;

public:
    explicit NameValueTableCalculator(QObject* parent = nullptr);
    virtual ~NameValueTableCalculator() override;

    Q_INVOKABLE void updateColumnWidths();

    virtual void classBegin() override {}
    virtual void componentComplete() override;

    static void registerQmlType();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
