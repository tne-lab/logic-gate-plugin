/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2016 Open Ephys

    Modified by:

    Alessio Buccino     alessiob@ifi.uio.no
    Mikkel Lepperod
    Svenn-Arne Dragly

    Center for Integrated Neuroplasticity CINPLA
    Department of Biosciences
    University of Oslo
    Norway

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>

#include "LogicGate.h"
#include "LogicGateEditor.h"


LogicGate::LogicGate()
    : GenericProcessor ("Logic Gate"),
      logicOp(0),
      outputChan(0),
      window(DEF_WINDOW),
      input1(-1),
      input2(-1),
      input1gate(false),
      input2gate(false)
{
    setProcessorType (PROCESSOR_TYPE_FILTER);
}

LogicGate::~LogicGate()
{
}

AudioProcessorEditor* LogicGate::createEditor()
{
    editor = new LogicGateEditor (this, true);
    return editor;
}

void LogicGate::createEventChannels()
{
    EventChannel* ev = new EventChannel(EventChannel::TTL, 8, 1, CoreServices::getGlobalSampleRate(), this);
    ev->setName("Logic Gate TTL output" );
    ev->setDescription("Triggers when logic operator is satisfied.");
    ev->setIdentifier ("dataderived.logicgate.trigger");
    eventChannelArray.add (ev);
}

void LogicGate::handleEvent (const EventChannel* eventInfo, const MidiMessage& event, int sampleNum)
{
    if (Event::getEventType(event) == EventChannel::TTL)
    {
        TTLEventPtr ttl = TTLEvent::deserializeFromMessage(event, eventInfo);
        const int state         = ttl->getState() ? 1 : 0;
        const int eventId       = ttl->getSourceIndex();
        const int sourceId      = ttl->getSourceID();
        const int eventChannel  = ttl->getChannel();

        if (input1 != -1)
        {
            EventSources s = sources.getReference (input1);
            if (eventId == s.eventIndex && sourceId == s.sourceId
                    && eventChannel == s.channel && state)
            {
                std::cout << "Received A " << std::endl;
                A = true;

                if ((input1gate) || (!input1gate && !input2gate))
                    m_previousTime = Time::currentTimeMillis();
            }
        }

        if (input2 != -1)
        {
            EventSources s = sources.getReference (input2);
            if (eventId == s.eventIndex && sourceId == s.sourceId
                    && eventChannel == s.channel && state)
            {
                std::cout << "Received B " << std::endl;
                B = true;
                if ((input2gate) || (!input1gate && !input2gate))
                    m_previousTime = Time::currentTimeMillis();
            }
        }
    }
}


void LogicGate::setInput1(int i1)
{
    input1 = i1;
}
void LogicGate::setInput2(int i2)
{
    input2 = i2;
}
void LogicGate::setGate1(bool set)
{
    input1gate = set;
}
void LogicGate::setGate2(bool set)
{
    input2gate = set;
}
void LogicGate::setLogicOp(int op)
{
    logicOp = op;
}
void LogicGate::setOutput(int out)
{
    outputChan = out;
}
void LogicGate::setWindow(int win)
{
    window = win;
}

void LogicGate::process (AudioSampleBuffer& buffer)
{
    checkForEvents ();
    // implement logic
    m_currentTime = Time::currentTimeMillis();
    m_timePassed = float(m_currentTime - m_previousTime);

    switch (logicOp)
    {
    case 0:
        //AND: as soon as AND is true send TTL output
        if (m_timePassed < window)
        {
            if (A && B)
            {
                std::cout << "AND condition satisfied ";
                int64 timestamp = CoreServices::getGlobalTimestamp();
                setTimestampAndSamples(timestamp, 0);
                uint8 ttlData = 1 << outputChan;
                const EventChannel* chan = getEventChannel(getEventChannelIndex(0, getNodeId()));
                TTLEventPtr event = TTLEvent::createTTLEvent(chan, timestamp, &ttlData, sizeof(uint8), outputChan);
                addEvent(chan, event, 0);

                if ((input1gate == input2gate))
                {
                    std::cout << "resetting input" << std::endl;
                    A = false;
                    B = false;
                }
                else if (!input1gate)
                {
                    std::cout << "resetting A" << std::endl;
                    A = false;
                }
                else if (!input2gate)
                {
                    std::cout << "resetting B" << std::endl;
                    B = false;
                }
            }
        }
        else
        {
            A = false;
            B = false;
        }
        break;

    case 1:
        //OR: if OR is true send TTL at the end of thw window
        if (m_timePassed > window)
        {
            if (A || B)
            {
                std::cout << "OR condition satisfied: resetting input" << std::endl;
                int64 timestamp = CoreServices::getGlobalTimestamp();
                setTimestampAndSamples(timestamp, 0);
                uint8 ttlData = 1 << outputChan;
                const EventChannel* chan = getEventChannel(getEventChannelIndex(0, getNodeId()));
                TTLEventPtr event = TTLEvent::createTTLEvent(chan, timestamp, &ttlData, sizeof(uint8), outputChan);
                addEvent(chan, event, 0);
                A = false;
                B = false;
            }
            else
            {
                A = false;
                B = false;
            }
            m_previousTime = Time::currentTimeMillis();
        }
        break;

    case 2:
        //XOR: if XOR is true send TTL at the end of thw window
        if (m_timePassed > window)
        {
            if (A != B)
            {
                std::cout << "XOR condition satisfied: resetting input" << std::endl;
                int64 timestamp = CoreServices::getGlobalTimestamp();
                setTimestampAndSamples(timestamp, 0);
                uint8 ttlData = 1 << outputChan;
                const EventChannel* chan = getEventChannel(getEventChannelIndex(0, getNodeId()));
                TTLEventPtr event = TTLEvent::createTTLEvent(chan, timestamp, &ttlData, sizeof(uint8), outputChan);
                addEvent(chan, event, 0);

                A = false;
                B = false;
            }
            else if ((A == B) && (A))
            {
                std::cout << "XOR condition NOT satisfied: resetting input" << std::endl;
                A = false;
                B = false;
            }
            else
            {
                A = false;
                B = false;
            }
            m_previousTime = Time::currentTimeMillis();
        }
        break;

    case 3: //DELAY
        if (m_timePassed > window)
        {
            if (A)
            {
                std::cout << "DELAY A" << std::endl;
                int64 timestamp = CoreServices::getGlobalTimestamp();
                setTimestampAndSamples(timestamp, 0);
                uint8 ttlData = 1 << outputChan;
                const EventChannel* chan = getEventChannel(getEventChannelIndex(0, getNodeId()));
                TTLEventPtr event = TTLEvent::createTTLEvent(chan, timestamp, &ttlData, sizeof(uint8), outputChan);
                addEvent(chan, event, 0);

                A = false;
            }
            m_previousTime = Time::currentTimeMillis();
        }
        break;
    }
}

void LogicGate::addEventSource(EventSources s)
{
    sources.add (s);
}

void LogicGate::clearEventSources()
{
    sources.clear();
}


void LogicGate::saveCustomParametersToXml(XmlElement *parentElement)
{
//    XmlElement* mainNode = parentElement->createNewChildElement("LogicGate");
//    for (int i=0; i<PULSEPALCHANNELS; i++)
//    {
//        XmlElement* chan = new XmlElement(String("Channel_")+=String(i+1));
//        chan->setAttribute("id", i);
//        chan->setAttribute("biphasic", m_isBiphasic[i]);
//        chan->setAttribute("phase1", m_phase1Duration[i]);
//        chan->setAttribute("phase2", m_phase1Duration[i]);
//        chan->setAttribute("interphase", m_interPhaseInterval[i]);
//        chan->setAttribute("voltage1", m_phase1Voltage[i]);
//        chan->setAttribute("voltage2", m_phase2Voltage[i]);
//        chan->setAttribute("restingvoltage", m_restingVoltage[i]);
//        chan->setAttribute("interpulse", m_interPulseInterval[i]);
//        chan->setAttribute("burstduration", m_burstDuration[i]);
//        chan->setAttribute("interburst", m_interBurstInterval[i]);
//        chan->setAttribute("trainduration", m_trainDuration[i]);
//        chan->setAttribute("traindelay", m_trainDelay[i]);
//        chan->setAttribute("link2trigger1", m_linkTriggerChannel1[i]);
//        chan->setAttribute("link2trigger2", m_linkTriggerChannel2[i]);
//        chan->setAttribute("triggermode", m_triggerMode[i]);
//        mainNode->addChildElement(chan);
//    }
}

void LogicGate::loadCustomParametersFromXml ()
{
//    if (parametersAsXml != nullptr)
//    {
//        forEachXmlChildElement (*parametersAsXml, mainNode)
//        {
//            if (mainNode->hasTagName ("LogicGate"))
//            {
//                forEachXmlChildElement(*mainNode, chan)
//                {
//                    int id = chan->getIntAttribute("id");
//                    int biphasic = chan->getIntAttribute("biphasic");
//                    double phase1 = chan->getDoubleAttribute("phase1");
//                    double phase2 = chan->getDoubleAttribute("phase2");
//                    double interphase = chan->getDoubleAttribute("interphase");
//                    double voltage1 = chan->getDoubleAttribute("voltage1");
//                    double voltage2 = chan->getDoubleAttribute("voltage2");
//                    double resting = chan->getDoubleAttribute("restingvoltage");
//                    double interpulse = chan->getDoubleAttribute("interpulse");
//                    double burst = chan->getDoubleAttribute("burstduration");
//                    double interburst = chan->getDoubleAttribute("interburst");
//                    double trainduration = chan->getDoubleAttribute("trainduration");
//                    double traindelay = chan->getDoubleAttribute("traindelay");
//                    int link21 = chan->getIntAttribute("link2trigger1");
//                    int link22 = chan->getIntAttribute("link2trigger2");
//                    int trigger = chan->getIntAttribute("triggermode");
//                    m_isBiphasic[id] = biphasic;
//                    m_phase1Duration[id] = phase1;
//                    m_phase2Duration[id] = phase2;
//                    m_interPhaseInterval[id] = interphase;
//                    m_phase1Voltage[id] = voltage1;
//                    m_phase2Voltage[id] = voltage2;
//                    m_restingVoltage[id] = resting;
//                    m_interPulseInterval[id] = interpulse;
//                    m_burstDuration[id] = burst;
//                    m_interBurstInterval[id] = interburst;
//                    m_trainDuration[id] = trainduration;
//                    m_trainDelay[id] = traindelay;
//                    m_linkTriggerChannel1[id] = link21;
//                    m_linkTriggerChannel2[id] = link22;
//                    m_triggerMode[id] = trigger;
//                }
//            }
//        }
//    }
}
