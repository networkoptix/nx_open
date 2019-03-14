
import QtQuick 2.1;

import "../base" as Base;

Base.IpControl
{
    property string startAddress: "";
    property int addressesCount: 0;
    readonly property bool valid: (initialText.length !== 0);

    initialText: impl.addToString(startAddress, addressesCount);

    enabled: false;


    property QtObject impl: QtObject
    {
        readonly property real kMaxAddress: Math.pow(2, 32);
        readonly property int kInvalidAddress: -1;
        readonly property int kClearMask: 255;

        function toInt(value, shift)
        {
            return (parseInt(value, 10) << shift) >>> 0;
        }

        function toIntIp(address)
        {
            var octets = address.split('.');
            if (octets.length !== 4)
                return kInvalidAddress;

            var result = toInt(octets[0], 24)
                + toInt(octets[1], 16) + toInt(octets[2], 8)
                + parseInt(octets[3]);

            return (isNaN(result) ? kInvalidAddress : result);
        }

        function toStringIp(address)
        {
            if (address == kInvalidAddress)
                return '';

            var firstOctet = (address >> 24) & kClearMask;
            var secondOctet = (address >> 16) & kClearMask;
            var thirdOctet = (address >> 8) & kClearMask;
            var fourthOctet = address & kClearMask;

            return firstOctet + '.' + secondOctet + '.' + thirdOctet + '.' + fourthOctet;
        }

        function addToString(startAddress, count)
        {
            var addr = toIntIp(startAddress);
            if (addr == kInvalidAddress)
                return '';

            var newAddr = addr + count - 1;
            if (newAddr >= kMaxAddress)
                return '';

            return toStringIp(newAddr);
        }
    }
}
