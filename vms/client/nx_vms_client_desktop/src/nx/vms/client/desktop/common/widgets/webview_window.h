// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QDialog>

#include <nx/utils/impl_ptr.h>

class QQuickItem;
namespace nx::vms::client::desktop {

class WebViewController;

class WebViewWindow: public QDialog
{
    Q_OBJECT
    using base_type = QDialog;

public:
    WebViewWindow(QWidget* parent);
    virtual ~WebViewWindow() override;

    WebViewController* controller() const;

signals:
    void rootReady(QQuickItem* root);

public slots:
    void loadAndShow();
    void setWindowGeometry(const QRect& geometry);
    void setWindowSize(int width, int height);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::desktop
