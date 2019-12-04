#pragma once

#include <vector>

#include <QtCore/QString>

namespace nx::vms::testcamera {

/**
 * Matrix defining the relative visual layout of channels of a multi-channel video file.
 */
class VideoLayout
{
public:
    /** Makes default layout: horizontal location of channels with increasing channel number. */
    explicit VideoLayout(int channelCount);

    /**
     * Parses a rectangular matrix from a string like `chN{,chN}{/chN{,chN}}`, where `/` separates
     * rows.
     *
     * @param outErrorMessage On error, receives the message in English, otherwise remains intact.
     */
    VideoLayout(const QString& slashCommaString, QString* outErrorMessage);

    int maxChannelNumber() const { return m_maxChannelNumber; }

    /**
     * Converts to a string like `width=<w>&height=<h>&sensors=<chN>{,<chN>}`, which can be parsed
     * by QnCustomResourceVideoLayout after replacing `&` with `;`.
     *
     * NOTE: Such format with `&` is used for transmitting VideoLayout from testcamera to a Server
     * via discovery response message, because `;` is used in this message to separate cameras.
     *
     * If the internal data is invalid, fails an assertion and returns a hard-coded string.
     */
    QByteArray toUrlParamString() const;

private:
    void parseSlashCommaString(const QString& slashCommaString, QString* outErrorMessage);

private:
    std::vector<std::vector<int>> m_sensors;
    int m_maxChannelNumber = -1;
};

} // namespace nx::vms::testcamera
