#pragma once

#include <QtWidgets/QPushButton>

#include <core/resource/resource_fwd.h>


class QnSelectDevicesButton : public QPushButton
{
    Q_OBJECT
    Q_ENUMS(Mode)
    using base_type = QPushButton;

public:
    QnSelectDevicesButton(QWidget* parent = nullptr);

    enum Mode
    {
        Any,
        All,
        Selected
    };

    Mode mode() const;
    void setMode(Mode mode);

    const QnVirtualCameraResourceList& selectedDevices() const;
    void setSelectedDevices(const QnVirtualCameraResourceList& list);

    struct SingleDeviceParameters
    {
        bool showName;
        bool showState;

        SingleDeviceParameters(bool showName = false, bool showState = false);
        bool operator == (const SingleDeviceParameters& other) const;
    };

    SingleDeviceParameters singleDeviceParameters() const;
    void setSingleDeviceParameters(const SingleDeviceParameters& parameters);

private:
    void updateData();

private:
    Mode m_mode;
    QnVirtualCameraResourceList m_selectedDevices;
    SingleDeviceParameters m_singleDeviceParameters;
};
