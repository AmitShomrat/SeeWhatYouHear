#include "SeeWhatYouHear/LEDCommunication.h"
#include "SeeWhatYouHear/PluginProcessor.h"

namespace audio_plugin {
    std::string find_esp32_device() {
        #if defined(__linux__) || defined(__APPLE__)
            // Use Boost.Process to run `udevadm` and capture all serial devices
            bp::ipstream out;
            bp::system("udevadm info -q property -n /dev/ttyUSB0", bp::std_out > out);
        
            std::string line;
            std::string device_path;
            std::regex vendor_re("ID_VENDOR_ID=(10c4|303a)", std::regex::icase); // 10c4=Silabs, 303a=Espressif
            std::regex product_re("ID_MODEL_ID=(ea60|1001)", std::regex::icase);
            std::regex name_re("ID_MODEL=.*(CP2102|ESP32)", std::regex::icase);
        
            while (std::getline(out, line)) {
                if (std::regex_search(line, vendor_re) || std::regex_search(line, product_re) || std::regex_search(line, name_re)) {
                    device_path = "/dev/esp32-led"; // our consistent udev alias
                    break;
                }
            }
            if (device_path.empty()) throw std::runtime_error("ESP32 device not found via udev.");
            return device_path;
        
        #elif defined(_WIN32)
            bp::ipstream out;
            // List COM ports with WMI (requires PowerShell)
            bp::child c(
                "powershell -Command \"Get-WmiObject Win32_SerialPort | Select-String 'Silicon Labs|ESP32'\"",
                bp::std_out > out
            );
        
            std::string line;
            std::string port;
            std::regex com_re(R"(COM\d+)");
            while (std::getline(out, line)) {
                std::smatch match;
                if (std::regex_search(line, match, com_re)) {
                    port = R"(\\.\)" + match.str();
                    break;
                }
            }
            c.wait();
            if (port.empty()) throw std::runtime_error("ESP32 COM port not found.");
            return port;
        #else
            throw std::runtime_error("Unsupported OS");
        #endif
}

LEDCommunication::LEDCommunication(AudioPluginAudioProcessor* processor) 
    : juce::Thread("LEDCommunicationThread"), processorPointer(processor)
{   
    // initializePearsonData(); Pearson Test.
    // ------------------------------------------------------------

    // 1 Mode byte + 2 * Brightness byte (R/L) + 2 * 3 Color byte (R/L).

    // TODO:
    // 1. try/catch for getenv appropriate handling.
    // 2. whene should we fetch the device port string run time(LEDComm ctor) / compile time (constat header).  
    // 3. Clear all comment previous class changes.
    // 4. Fix github actions file:
    //     4.1 windows platform dependencies download is to comnplicated look the right way to perfoems it properlty
    //     4.2 Once the pipe is got stabled we have to try out the artifacts on Windows OS with the usage of the installation script.
    //     4.2 Need to use mac-os ( Darwin ) CI/CD building artifacts 
    // 5. Encapsulating the serial functions using async io thread (look GPT suggestion).
    // 6. Next design the structs / patterns for stage multiple led strips usage.
    // 7. Rearrange and add docs of LEDCommunication functions.

    // portName = std::getenv("ESP32_PORT");
    try{
        portName = find_esp32_device();
    }
    catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }





    serial_ = std::make_unique<boost::asio::serial_port>(io_); // Constracting port object with io context.

    ledData.resize(9);
    startThread();
}

void LEDCommunication::openSerial(unsigned baud) {
    if (serial_->is_open())
        return;

    boost::system::error_code ec;
    serial_->open(portName, ec);
    if (ec) throw std::runtime_error("open failed: " + ec.message());

    serial_->set_option(boost::asio::serial_port_base::baud_rate(baud), ec);
    serial_->set_option(boost::asio::serial_port_base::character_size(8), ec);
    serial_->set_option(boost::asio::serial_port_base::parity(
                            boost::asio::serial_port_base::parity::none), ec);
    serial_->set_option(boost::asio::serial_port_base::stop_bits(
                            boost::asio::serial_port_base::stop_bits::one), ec);
    serial_->set_option(boost::asio::serial_port_base::flow_control(
                            boost::asio::serial_port_base::flow_control::none), ec);
    if (ec) throw std::runtime_error("configure failed: " + ec.message());
}

void LEDCommunication::closeSerial() {
    if (!serial_) return;
    boost::system::error_code ec;
    if (serial_->is_open()) {
        serial_->cancel(ec);
        serial_->close(ec);
    }
}

LEDCommunication::~LEDCommunication() 
{
    stopThread(1500);
    if(serial_ -> is_open()){
        // Turn off LEDs before closing
        std::cout << "Turning off LEDs before closing." << std::endl;
        ledData[0] = static_cast<unsigned char>(4);
        writeBytes(ledData.data(), ledData.size());
        closeSerial();
        std::cout << "LEDCommunication destroyed." << std::endl;
    }
}

int LEDCommunication::setBrightness(float monoBrightness)
{ 
    float monoChannelScaled = 0.0f;

    // Add threshold check for zero values
    if (std::abs(monoBrightness) < 0.001f) {
        monoChannelScaled = 0.0f;
    } else {
        // Use decibels scaling with expanded range (-60dB to 0dB)
        monoChannelScaled = monoBrightness;
    }
    // Apply power curve for better sensitivity at low levels
    // float memory = 0.9898f sensetive for low values start leds at -40Db due to the totalyzor
    // float memory2 = 3.0f sensetive for high values start leds at -10Db due to the totalyzor
    monoChannelScaled = std::pow(monoChannelScaled, 0.5f);

    // Map to 0-255 range with threshold at 0.004
    float mappedValueLeft = juce::jmap(
        monoChannelScaled, 
        0.0f,
        1.0f,
        0.0f,
        255.0f
    );

    // Store the brightness values with threshold applied
    return juce::jlimit(0, 255, static_cast<int>(std::round(mappedValueLeft)));
}

void LEDCommunication::prepareData(RGB rgbLeftValues, RGB rgbRightValues)
{
    if (processorPointer) {
        currentLeftBrightness.store(setBrightness(processorPointer->getBrightnessDecision().getLeftBrightness().get()));
        currentRightBrightness.store(setBrightness(processorPointer->getBrightnessDecision().getRightBrightness().get()));
    }

    RGB scaledRGBLeft{
        static_cast<uint8_t>(rgbLeftValues.r * (currentLeftBrightness.load() / 255.0f)),
        static_cast<uint8_t>(rgbLeftValues.g * (currentLeftBrightness.load() / 255.0f)),
        static_cast<uint8_t>(rgbLeftValues.b * (currentLeftBrightness.load() / 255.0f))
    };
    // std::cout << "scaledRGBLeft: " << static_cast<int> (scaledRGBLeft.r) << " " << static_cast<int> (scaledRGBLeft.g) << " " << static_cast<int> (scaledRGBLeft.b) << std::endl;
    // std::cout << "leftScale: " << leftScale << std::endl;
    RGB scaledRGBRight{
        static_cast<uint8_t>(rgbRightValues.r * (currentRightBrightness.load() / 255.0f)),
        static_cast<uint8_t>(rgbRightValues.g * (currentRightBrightness.load() / 255.0f)),
        static_cast<uint8_t>(rgbRightValues.b * (currentRightBrightness.load() / 255.0f))
    };
    // std::cout << "scaledRGBRight: " << static_cast<int> (scaledRGBRight.r) << " " << static_cast<int> (scaledRGBRight.g) << " " << static_cast<int> (scaledRGBRight.b) << std::endl;
    // std::cout << "rightScale: " << rightScale << std::endl;
    
    currentMode.store(static_cast<int>(processorPointer->apvts.getRawParameterValue("Mode")->load()));
    ledData[0] = static_cast<unsigned char>(currentMode.load());
    // Why not sending only mode bit, color and brightness and the LED itself will duplicate the color for each led?
    ledData[1] = static_cast<unsigned char>(currentLeftBrightness.load());
    ledData[2] = static_cast<unsigned char>(currentRightBrightness.load());
    ledData[3] = static_cast<unsigned char>(scaledRGBLeft.r);
    ledData[4] = static_cast<unsigned char>(scaledRGBLeft.g);
    ledData[5] = static_cast<unsigned char>(scaledRGBLeft.b);
    ledData[6] = static_cast<unsigned char>(scaledRGBRight.r);
    ledData[7] = static_cast<unsigned char>(scaledRGBRight.g);
    ledData[8] = static_cast<unsigned char>(scaledRGBRight.b);
}


std::size_t LEDCommunication::writeBytes(const uint8_t* data, std::size_t n) {
    if (!serial_ || !serial_->is_open()) return 0;
    boost::system::error_code ec;
    auto written = boost::asio::write(*serial_, boost::asio::buffer(data, n), ec);
    if (ec) { return 0; /*handle log*/}
    return written;
}

std::size_t LEDCommunication::readSome(uint8_t* dst, std::size_t max) {
    if (!serial_ || !serial_->is_open()) return 0;
    boost::system::error_code ec;
    auto n = serial_->read_some(boost::asio::buffer(dst, max), ec);
    if (ec) {/* handle/log */}
    return n;
}



void LEDCommunication::sendData() {
    if (processorPointer) {
        prepareData(processorPointer->fftProcessor->getLeftRGB(),
                    processorPointer->fftProcessor->getRightRGB());
 
        // logPearsonData(); Pearson Test.
        // ------------------------------------------------------------
        if(!writeBytes(ledData.data(), ledData.size())){
            std::cout << "Failed to write to serial port." << std::endl;
            return;
        }
    }

    // Need to handle missing processor pointer.
}


bool LEDCommunication::checkConnection(){
    if ( !serial_ || ( !serial_->is_open() && juce::Time::currentTimeMillis() - lastRetryTime > RETRY_INTERVAL_MS)) {
            lastRetryTime = juce::Time::currentTimeMillis();
            
            std::cout << "Attempting to connect to port: " << portName << " at " << juce::Time::getCurrentTime().toString(true, true) << std::endl;

            try { openSerial(115200);} 
            catch (const std::exception& e) {
                juce::ignoreUnused(e);
                std::cout << "Connection attempt failed - will retry in " << RETRY_INTERVAL_MS/1000 << " seconds" << std::endl;
                return false;
            }

            std::cout << "Successfully connected to port: " << portName << std::endl;
        }

        if (!serial_->is_open()) {
            wait(100);
            return false;
        }
    return true;
}


void LEDCommunication::run() {
    std::cout << "=== LED Communication Thread Started ===" << std::endl;
    std::cout << "Initial port: " << portName << std::endl;
    
    try { openSerial(115200);} 
    catch (const std::exception& e) {
        juce::ignoreUnused(e);
        std::cout << "Failed to establish port" << portName << std::endl;
        return;
    }
    
    startTimeMs = juce::Time::getMillisecondCounter();
    while (!threadShouldExit()) {
        if(!checkConnection()) continue;
        //------------------------------run operation--------------------------------
        sendData();
        
        // readSerialData(); Pearson Test.
        wait(16);
    }
    std::cout << "=== LED Communication Thread Stopping ===" << std::endl;
}







// void LEDCommunication::initializePearsonData()
// {
// #ifdef _WIN32
//     // Go one level up from AUDIO_PRO directory
//     std::string basePath = "C:\\Users\\amit5\\Desktop\\AUDIO_PRO\\audio-plugin-template";
//     std::string pearsonPath = basePath + "\\PearsonData";
    
//     // Create directory using Windows API
//     if (CreateDirectoryA(pearsonPath.c_str(), NULL) || 
//         GetLastError() == ERROR_ALREADY_EXISTS) {
        
//         // Create/open the DSP data CSV file
//         std::string dspPath = pearsonPath + "\\dspData.csv";
//         pearsonDataFile.open(dspPath, std::ios::out);
        
//         // Create/open the ESP32 data CSV file
//         std::string esp32Path = pearsonPath + "\\ESP32Data.csv";
//         esp32DataFile.open(esp32Path, std::ios::out);
        
//         if (pearsonDataFile.is_open() && esp32DataFile.is_open()) {
//             // Write headers to both files
//             pearsonDataFile << "timestamp_ms,left_brightness,right_brightness\n";
//             esp32DataFile << "timestamp_ms,left_brightness,right_brightness\n";
//         } else {
//             std::cout << "Failed to create/open one or both CSV files" << std::endl;
//             if (!pearsonDataFile.is_open()) std::cout << "Failed to open dspData.csv" << std::endl;
//             if (!esp32DataFile.is_open()) std::cout << "Failed to open ESP32Data.csv" << std::endl;
//         }
//     } else {
//         std::cout << "Failed to create PearsonData directory! Error: " << GetLastError() << std::endl;
//     }
// #else
//     // Linux version - use current directory for now
//     std::string pearsonPath = "PearsonData";
    
//     // Create directory using mkdir
//     if (mkdir(pearsonPath.c_str(), 0755) == 0 || errno == EEXIST) {
        
//         // Create/open the DSP data CSV file
//         std::string dspPath = pearsonPath + "/dspData.csv";
//         pearsonDataFile.open(dspPath, std::ios::out);
        
//         // Create/open the ESP32 data CSV file
//         std::string esp32Path = pearsonPath + "/ESP32Data.csv";
//         esp32DataFile.open(esp32Path, std::ios::out);
        
//         if (pearsonDataFile.is_open() && esp32DataFile.is_open()) {
//             // Write headers to both files
//             pearsonDataFile << "timestamp_ms,left_brightness,right_brightness\n";
//             esp32DataFile << "timestamp_ms,left_brightness,right_brightness\n";
//         } else {
//             std::cout << "Failed to create/open one or both CSV files" << std::endl;
//             if (!pearsonDataFile.is_open()) std::cout << "Failed to open dspData.csv" << std::endl;
//             if (!esp32DataFile.is_open()) std::cout << "Failed to open ESP32Data.csv" << std::endl;
//         }
//     } else {
//         std::cout << "Failed to create PearsonData directory! Error: " << strerror(errno) << std::endl;
//     }
// #endif
// }

// void LEDCommunication::logPearsonData()
// {
//     juce::uint32 currentTimeMs = juce::Time::getMillisecondCounter();
//     if (pearsonDataFile.is_open()) {
//         juce::uint32 elapsedMs = currentTimeMs - (startTimeMs + 37);
        
//         pearsonDataFile << elapsedMs << "," 
//                        << currentLeftBrightness.load() << "," 
//                        << currentRightBrightness.load() << "\n";
//     }
// }   


} // namespace audio_plugin