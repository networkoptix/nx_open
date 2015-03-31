using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using NxInterface;
using AxInterop.hdwitness;

namespace NxControl
{
    public class NxControl23 : AxAxHDWitness, IPlugin
    {
        public NxControl23() {
            base.connectionProcessed += axAxHDWitness1_connectedProcessed;
        }

        public event ConnectionProcessedEventHandler connectProcessed;
        void axAxHDWitness1_connectedProcessed(object sender, IAxHDWitnessEvents_connectionProcessedEvent e) {
            connectProcessed(sender, new ConnectionProcessedEvent(e.p_status, e.p_message));
        }
    }
}
