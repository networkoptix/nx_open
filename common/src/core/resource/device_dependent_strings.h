#pragma once

#include <array>

#include <core/resource/resource_fwd.h>

enum QnCameraDeviceType {
    Mixed,
    Camera,
    IOModule,

    Count
};

class QnCameraDeviceStringSet {
public:
    QnCameraDeviceStringSet();
    QnCameraDeviceStringSet(
          const QString &mixedString
        , const QString &cameraString
        , const QString &ioModuleString = QString()
        );
    QnCameraDeviceStringSet(
          const QString &mixedSigularString
        , const QString &mixedPluralString
        , const QString &cameraSigularString
        , const QString &cameraPluralString
        , const QString &ioModuleSigularString
        , const QString &ioModulePluralString
        );

    QString getString(QnCameraDeviceType deviceType, bool plural) const;
    void setString(QnCameraDeviceType deviceType, bool plural, const QString &value);

    bool isValid() const;
private:
    std::array<QString, QnCameraDeviceType::Count> m_singularStrings;
    std::array<QString, QnCameraDeviceType::Count> m_pluralStrings;
};


class QnDeviceDependentStrings {
public:
    /**
    * @brief Calculate default devices name.
    * @details Following rules are applied:
    * * If all devices in the system are cameras - "Cameras";
    * * Otherwise "Devices" is used;
    * @param plural        Should the word be in the plural form, default value is true.
    * @param capitalize    Should the word begin from the capital letter, default value is true.
    */
    static QString getDefaultName(bool plural = true, bool capitalize = true);

    /**
    * @brief Calculate default devices name for the given devices list.
    * @details Following rules are applied:
    * * If all devices in the list are cameras - "Cameras";
    * * If all devices in the list are IO modules - "IO Modules";
    * * Otherwise "Devices" is used;
    * @param capitalize    Should the word begin from the capital letter, default value is true.
    */
    static QString getDefaultName(const QnVirtualCameraResourceList &devices, bool capitalize = true);

    /** Shortcut for single device or singular form. */
    static QString getDefaultNameLower(const QnVirtualCameraResourcePtr &device = QnVirtualCameraResourcePtr());

    /** Shortcut for single device or singular form. */
    static QString getDefaultNameUpper(const QnVirtualCameraResourcePtr &device = QnVirtualCameraResourcePtr());

    /**
    * @brief Calculate common name for the target devices list.
    * @details Following rules are applied:
    * * If all devices are cameras - "%n Cameras";
    * * If all devices are IO Modules - "%n IO Modules";
    * * For mixed list "%n Devices" is used;
    * @param capitalize Should the word begin from the capital letter, default value is true.
    */
    static QString getNumericName(const QnVirtualCameraResourceList &devices, bool capitalize = true);

    /**
    * @brief Select string from the given set based on the target devices list.
    */
    static QString getNameFromSet(const QnCameraDeviceStringSet &set, const QnVirtualCameraResourceList &devices);

    /**
    * @brief Select default string from the given set based on all devices in the system.
    */
    static QString getDefaultNameFromSet(const QnCameraDeviceStringSet &set);

    /**
    * @brief Select default string from the given set based on all devices in the system.
    */
    static QString getDefaultNameFromSet(const QString &mixedString, const QString &cameraString);
};