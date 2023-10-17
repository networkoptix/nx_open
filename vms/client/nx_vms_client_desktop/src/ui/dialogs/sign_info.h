// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimer>
#include <QtWidgets/QLabel>

#include <nx/utils/thread/mutex.h>
#include <ui/workbench/workbench_context_aware.h>

#include "export/sign_helper.h"

// TODO: #sivanov Not a dialog, a label. Move to widgets.
class QnSignInfo: public QLabel, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool signIsMatched READ signIsMatched)

public:
    QnSignInfo(QWidget* parent = nullptr);

    void setImageSize(int width, int height);
    void setDrawDetailTextMode(bool value); // draw detail text instead of match info
    bool signIsMatched();

protected:
    virtual void paintEvent(QPaintEvent *event) override;

public slots:
    void at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame);
    void at_gotSignatureDescription(QString version, QString hwId, QString licensedTo);
    void at_calcSignInProgress(QByteArray sign, int progress);

private:
    QnSignHelper m_signHelper;
    QByteArray m_sign;
    QByteArray m_signFromFrame;
    nx::Mutex m_mutex;
    bool m_finished;
    int m_progress;
    QTimer m_timer;
    bool m_DrawDetailText;
    bool m_signIsMatched = false;
};
