using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using NxInterface;
using AxInterop.DWSpectrum;

namespace NxControl
{
    public class NxControl : AxAxDWSpectrum, IPlugin
    {
        public NxControl() {
            base.connectionProcessed += axDWSpectrum1_connectedProcessed;
        }

        public event ConnectionProcessedEventHandler connectProcessed;
        void axDWSpectrum1_connectedProcessed(object sender, IAxDWSpectrumEvents_connectionProcessedEvent e) {
            connectProcessed(sender, new ConnectionProcessedEvent(e.p_status, e.p_message));
        }
    }
}
