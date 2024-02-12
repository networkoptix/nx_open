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

    UuidSet getResources() const;
    void setResources(const UuidSet& resources);

protected:
    virtual void handleButtonClicked() = 0;

signals:
    void commit();

private:
    UuidSet m_resources;
};
