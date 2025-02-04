#include "AsyncNETSGPClient.h"

#include <Arduino.h>

AsyncNETSGPClient::AsyncNETSGPClient(Stream& stream, const uint8_t progPin, const uint8_t interval)
    : NETSGPClient(stream, progPin), mIntervalMS(1000 * interval), mDeviceIte(mDevices.begin())
{ }

void AsyncNETSGPClient::update()
{
    const uint32_t currentMillis = millis();

    // Send comands at mIntervalMS
    if (currentMillis - mLastUpdateMS >= mIntervalMS && !mCanSend)
    {
        mCanSend = true;
    }

    if (mCanSend && currentMillis - mLastSendMS >= 1010)
    {
        if (mDeviceIte != mDevices.end())
        {
            mLastSendMS = currentMillis;
            mLastUpdateMS = currentMillis;
            sendCommand(*mDeviceIte, Command::STATUS);
            DEBUGF("Sent STATUS request to %#08x\n", *mDeviceIte);
            mCanSend = false;
            ++mDeviceIte;
        }
        else
        {
            mCanSend = false; // make sure we only poll every mIntervalMS
            mDeviceIte = mDevices.begin();
        }
    }

    // Check for answers
    while (mStream.available() >= 27)
    {
        // Search for status message
        if (findAndReadReply(Command::STATUS))
        {
            dumpBuffer();
            InverterStatus status;
            if (fillInverterStatusFromBuffer(&mBuffer[0], status))
            {
                if (mCallback)
                {
                    mCallback(status);
                }
                mCanSend = true;
            }
        }
    }
}

String AsyncNETSGPClient::getInvertersJSON(void) {
    String tmpStr = "{\"inverter\":{\"count\":";
    tmpStr+= String(mDevices.size());
    if ( mDevices.size() > 0 ) {
        tmpStr+= ",\"inverterID\":[";
        for(auto id : mDevices) {
            char devID[10];
            sprintf(devID, "%08lX", id);
            tmpStr+= "\"" + String(devID) + "\",";
        }
        if (tmpStr.length () > 0) tmpStr.remove (tmpStr.length()-1, 1);
        tmpStr+= "]";
    }
    
    tmpStr+= "}}";
    return tmpStr;
}
