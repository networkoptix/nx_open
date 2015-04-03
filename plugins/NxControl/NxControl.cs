using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using NxInterface;
using AxInterop.NxWitness;

namespace NxControl
{
    public class NxControl : AxAxNxWitness, IPlugin
    {
        public NxControl() {
            base.connectionProcessed += axNxWitness1_connectedProcessed;
        }

        public event ConnectionProcessedEventHandler connectProcessed;
        void axNxWitness1_connectedProcessed(object sender, IAxNxWitnessEvents_connectionProcessedEvent e)
        {
            connectProcessed(sender, new ConnectionProcessedEvent(e.p_status, e.p_message));
        }
    }
}
