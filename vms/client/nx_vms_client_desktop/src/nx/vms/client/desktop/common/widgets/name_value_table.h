#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class NameValueTable: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    NameValueTable(QWidget* parent = nullptr);
    virtual ~NameValueTable() override;

    using NameValueList = QList<QPair<QString, QString>>;

    NameValueList content() const;

    // Warning! Calling setContent during update of a backing store of some OpenGL-backed window
    // must be avoided as OpenGL context switching at that moment can cause bugs at some systems.
    void setContent(const NameValueList& value);

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    virtual void paintEvent(QPaintEvent* event) override;

    virtual bool event(QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
