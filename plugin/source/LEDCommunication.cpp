#include "SimpleEQ/LEDCommunication.h"
#include <windows.h>

LEDCommunication::LEDCommunication(const juce::String& portName) 
: juce::Thread("LEDCommunicationTread"), portName(portName)
{
    startThread();
}

LEDCommunication::~LEDCommunication() 
{
  stopThread(1000);
}



void LEDCommunication::run()
{

    HANDLE hserial = CreateFile(portName.toRawUTF8(), 
                                GENERIC_WRITE,
                                0, 
                                NULL, 
                                OPEN_EXISTING, 
                                0, 
                                NULL );

    if(hserial == INVALID_HANDLE_VALUE)
    {
        DBG("Failed to open serial port.");
        return;
    }
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(hserial, &dcb);
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(hserial, &dcb);


    const int numLEDs = 300;
    std::vector<unsigned char> ledData(numLEDs * 3 + 1 + 1);
    while(!threadShouldExit())
    {
        ledData[0] = static_cast<unsigned char>(0xFF);
        auto brightness = currentBrightness.load();
        ledData[1] = static_cast<unsigned char>(brightness);

        switch(currentColor.load()){
            case Color::Red:
                for(int i = 0; i < 149; ++i)
            {
                ledData[i * 3 + 2] = 0;
                ledData[i * 3 + 3] = 0;
                ledData[i * 3 + 4] = 0;
            }

            for(int i = 150; i < 299; ++i)
            {
                ledData[i * 3 + 2] = 0;
                ledData[i * 3 + 3] = 0;
                ledData[i * 3 + 4] = 0;
            }        
            break;

            case Color::Blue:
                for(int i = 0; i < 149; ++i)
            {
                ledData[i * 3 + 2] = 0;
                ledData[i * 3 + 3] = 0;
                ledData[i * 3 + 4] = 255;
            }

            for(int i = 150; i < 299; ++i)
            {
                ledData[i * 3 + 2] = 0;
                ledData[i * 3 + 3] = 0;
                ledData[i * 3 + 4] = 255;
            }
            break;

            case Color::Green:
            for(int i = 0; i < 149; ++i)
            {
                ledData[i * 3 + 2] = 0;
                ledData[i * 3 + 3] = 255;
                ledData[i * 3 + 4] = 0;
            }

            for(int i = 150; i < 299; ++i)
            {
                ledData[i * 3 + 2] = 0;
                ledData[i * 3 + 3] = 255;
                ledData[i * 3 + 4] = 0;
            }
            break;

            case Color::Yellow:
                for(int i = 0; i < 149; ++i)
            {
                ledData[i * 3 + 2] = 255;
                ledData[i * 3 + 3] = 255;
                ledData[i * 3 + 4] = 0;
            }

            for(int i = 150; i < 299; ++i)
            {
                ledData[i * 3 + 2] = 255;
                ledData[i * 3 + 3] = 255;
                ledData[i * 3 + 4] = 0;
            }
            break;       
        }

            DWORD bytesWritten;
            // unsigned char sync = 0xFF;
            // WriteFile(hserial, &sync, 1, &bytesWritten, NULL);
            WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL);
    }
            CloseHandle(hserial);
}

