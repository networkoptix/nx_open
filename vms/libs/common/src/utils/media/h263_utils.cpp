#include "h263_utils.h"

#include <utils/media/bitStream.h>

namespace nx::media_utils::h263 {

// based on ff_h263_decode_picture_header
bool PictureHeader::decode(const uint8_t* data, uint32_t size)
{
    try {
        BitStreamReader reader(data, data + size);
        uint32_t startcode = reader.getBits(22);
        if (startcode != 0x20)
            return false;

        timestamp = reader.getBits(8);
        if (!reader.getBit()) // Marker bit, should be 1
            return false;

        if (reader.getBit()) // H263 id, should be 0
            return false;

        reader.skipBit(); /* split screen off */
        reader.skipBit(); /* camera  off */
        reader.skipBit(); /* freeze picture release off */
        format = reader.getBits(3);
        if (format != 7 && format != 6)
        {
            /* H.263v1 */
            pictureType = reader.getBit() ? PictureType::I : PictureType::P;
        }
        else
        {
            /* H.263v2 */
            uint32_t ufep = reader.getBits(3); /* Update Full Extended PTYPE */

            /* ufep other than 0 and 1 are reserved */
            if (ufep == 1) {
                /* OPPTYPE */
                format = reader.getBits(3);
                reader.skipBits(15); // TODO process skipped bits
            }
            else if (ufep != 0)
            {
                return false;
            }
            /* MPPTYPE */
            uint32_t pt = reader.getBits(3);
            switch(pt)
            {
                case 0:
                case 7: //ZYGO
                    pictureType = PictureType::I;
                    break;
                case 1:
                case 2:
                    pictureType = PictureType::P;
                    break;
                case 3:
                    pictureType = PictureType::B;
                default:
                    return false;
            }
        }
    }
    catch(const BitStreamException&)
    {
        return false;
    }
    return true;

}

} // namespace nx::media_utils::h263
