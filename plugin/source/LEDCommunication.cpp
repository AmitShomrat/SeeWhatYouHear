#include "SimpleEQ/LEDCommunication.h"
#include <windows.h>

LEDCommunication::LEDCommunication(const juce::String& portName) 
: juce::Thread("LEDCommunicationTread"), portName(portName)
{   
    //Data size: 1 (sync), 2 (brightnesses), 900 (RGB colors),
    ledData.resize(numLEDs * 3 + 1 + 2);
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


    DWORD bytesWritten;
    while(!threadShouldExit())
    {
        switch(currentColor.load()){
            case Color::Red:
            prepareData(RGB{255, 0, 0}, RGB{255, 0, 0});        
            break;

            case Color::Blue:
            prepareData(RGB{0, 0, 255}, RGB{0, 0, 255});        
            break;

            case Color::Green:
            prepareData(RGB{0, 255, 0}, RGB{0, 255, 0});        
            break;

            case Color::Yellow:
            prepareData(RGB{255, 255, 0}, RGB{255, 255, 0});        
            break;
        }
        WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL);
    }
    //Turn off all LEDs
    prepareData(RGB{0, 0, 0}, RGB{0, 0, 0});
    WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL);


    CloseHandle(hserial);
}

