#include "video_layout.h"

#include <QtCore/QString>

#include <nx/kit/utils.h>
#include <nx/utils/log/log.h>
#include <nx/streaming/config.h> //< For CL_MAX_CHANNELS

namespace nx::vms::testcamera {

static QString enquoteAndEscape(const QString& value)
{
    return QString::fromStdString(nx::kit::utils::toString(value.toStdString()));
}

VideoLayout::VideoLayout(int channelCount)
{
    if (!NX_ASSERT(channelCount > 0 && channelCount <= CL_MAX_CHANNELS,
        "channelCount: %1.", channelCount))
    {
        return;
    }

    m_sensors.resize(1); //< Single row.
    m_sensors.at(0).resize(channelCount);
    for (int channelNumber = 0; channelNumber < channelCount; ++channelNumber)
        m_sensors.at(0).at(channelNumber) = channelNumber;

    m_maxChannelNumber = channelCount - 1;
}

VideoLayout::VideoLayout(const QString& slashCommaString, QString* outErrorMessage)
{
    if (!NX_ASSERT(outErrorMessage))
        return;

    parseSlashCommaString(slashCommaString, outErrorMessage);
}

void VideoLayout::parseSlashCommaString(const QString& slashCommaString, QString* outErrorMessage)
{
    const auto error = //< Intended to be called like `return error("...", ...);`.
        [outErrorMessage](const QString& message, auto... args)
        {
            if constexpr (sizeof...(args) > 0)
                *outErrorMessage = lm(message).args(args...);
            else
                *outErrorMessage = message;
        };

    std::set<int> channelNumbers;
    for (const QString& row: slashCommaString.split('/'))
    {
        std::vector<int> rowData;
        for (const QString& itemStr: row.split(','))
        {
            bool success = false;
            const int channelNumber = itemStr.toInt(&success);
            if (!success)
                return error("Must contain integers but %1 found.", enquoteAndEscape(itemStr));

            if (channelNumber < 0 || channelNumber >= CL_MAX_CHANNELS)
                return error("Value range is 0 to %1 but %2 found.", CL_MAX_CHANNELS - 1, itemStr);

            if (const auto [_, isNew] = channelNumbers.insert(channelNumber); !isNew)
                return error("Contains duplicate channelNumber %1.", channelNumber);

            rowData.push_back(channelNumber);
        }

        if (rowData.size() == 0)
            return error("The matrix contains an empty row.");

        m_sensors.push_back(rowData);

        if (rowData.size() != m_sensors.at(0).size())
            return error("The matrix has rows of different length.");
    }

    if (m_sensors.empty())
        return error("The matrix is empty.");

    m_maxChannelNumber = *channelNumbers.rbegin(); //< Last item of the sorted set.
}

QByteArray VideoLayout::toUrlParamString() const
{
    if (!NX_ASSERT(!m_sensors.empty()))
        return "EMPTY_VIDEO_LAYOUT";

    const int width = (int) m_sensors.at(0).size();
    const int height = (int) m_sensors.size();

    QByteArray result = lm("width=%1&height=%2&sensors=").args(width, height).toUtf8();

    for (int row = 0; row < height; ++row)
    {
        const std::vector<int>& rowData = m_sensors.at(row);

        const int rowWidth = (int) rowData.size();
        if (!NX_ASSERT(rowWidth == width, "Row %1 has uneven width %1.", row, rowWidth))
            return "INVALID_VIDEO_LAYOUT";

        for (int item = 0; item < width; ++item)
        {
            if (!(row == 0 && item == 0))
                result += ",";
            result += QByteArray::number(rowData.at(item));
        }
    }

    return result;
}

} // namespace nx::vms::testcamera
