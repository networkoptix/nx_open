// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtWidgets/QPushButton>

#include <nx/utils/uuid.h>

class QnSelectResourcesDialogButton : public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    explicit QnSelectResourcesDialogButton(QWidget* parent = nullptr);
    virtual ~QnSelectResourcesDialogButton() override;

    QnUuidSet getResources() const;
    void setResources(const QnUuidSet& resources);

protected:
    virtual void handleButtonClicked() = 0;

signals:
    void commit();

private:
    QnUuidSet m_resources;
};
