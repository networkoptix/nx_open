// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLabel>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

class QnResourceSelectionDialogDelegate:
    public QObject,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
public:
    explicit QnResourceSelectionDialogDelegate(QObject* parent = nullptr);
    ~QnResourceSelectionDialogDelegate();

    /**
     * @brief init                  This method allows delegate setup custom error or statistics widget
     *                              that will be displayed below the resources list.
     * @param parent                Parent container widget.
     */
    virtual void init(QWidget* parent);

    /**
     * @brief validate              This method allows delegate modify message depending on the selection
     *                              and control the OK button status.
     * @param selectedResources     List of selected resources.
     * @return                      True if selection is valid and OK button can be pressed.
     */
    virtual bool validate(const QSet<QnUuid>& selectedResources);
    virtual QString validationMessage(const QSet<QnUuid>& selectedResources) const;

    virtual bool isValid(const QnUuid& resource) const;

    /**
     * @brief isMultiChoiceAllowed  Check if the delegate allows to select several resources in the list.
     */
    virtual bool isMultiChoiceAllowed() const;

protected:
    QnResourcePtr getResource(const QnUuid& resource) const;
    QnResourceList getResources(const QSet<QnUuid>& resources) const;
};
