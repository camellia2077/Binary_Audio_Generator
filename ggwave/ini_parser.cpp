// ini_parser.cpp
#include "ini_parser.h" // Include the header we just defined
#include <fstream>
#include <iostream> // For cerr, cout, endl
#include <sstream>  // For std::stringstream
#include <algorithm> // For std::tolower (optional, if keys are case-insensitive)
#include <cstdlib>   // For exit, EXIT_FAILURE

// Helper function to trim whitespace from a string
static std::string trimStringIni(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

Config loadIniConfig(const std::string& filename) {
    Config config; // Initialize with default values from the struct definition
    std::ifstream configFile(filename);
    std::string line;

    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open config file '" << filename << "'. Program will exit." << std::endl;
        exit(EXIT_FAILURE); // Exit if INI file cannot be read
    }

    std::cout << "Loading configuration from: " << filename << std::endl;

    while (std::getline(configFile, line)) {
        line = trimStringIni(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') { // Skip empty lines and comments (';' or '#')
            continue;
        }

        size_t delimiterPos = line.find('=');
        if (delimiterPos == std::string::npos) {
            std::cerr << "Warning: Malformed line in config (no '='): " << line << std::endl;
            continue;
        }

        std::string key = trimStringIni(line.substr(0, delimiterPos));
        std::string valueStr = trimStringIni(line.substr(delimiterPos + 1));

        try {
            if (key == "SAMPLE_RATE") config.sampleRate = std::stoi(valueStr);
            else if (key == "BITS_PER_SAMPLE") config.bitsPerSample = std::stoi(valueStr);
            else if (key == "TONE_DURATION_S") config.toneDurationS = std::stof(valueStr);
            else if (key == "AMPLITUDE_SCALE") config.amplitude = std::stof(valueStr) * 32767.0f; // ensure float multiplication
            else if (key == "SILENCE_DURATION_S") config.silenceDurationS = std::stof(valueStr);
            else if (key == "START_TONE_FREQ") config.startToneFreq = std::stof(valueStr);
            else if (key == "END_TONE_FREQ") config.endToneFreq = std::stof(valueStr);
            else if (key == "SYNC_TONE_DURATION_S") config.syncToneDurationS = std::stof(valueStr);
            else if (key == "OUTPUT_WAV_FILENAME") config.outputWavFilename_config = valueStr;
            else if (key == "FREQ_TOLERANCE") config.freqTolerance = std::stof(valueStr); // New: Read frequency tolerance
            else if (key.rfind("CHAR_", 0) == 0 && key.length() > 5) { // Starts with "CHAR_"
                try {
                    // Expecting format CHAR_65=1000.0 (for 'A') or CHAR_A=1000.0
                    std::string charKeyPart = key.substr(5);
                    char actualChar;
                    if (charKeyPart.length() == 1 && !std::isdigit(static_cast<unsigned char>(charKeyPart[0]))) {
                        actualChar = charKeyPart[0];
                    } else {
                        int charCode = std::stoi(charKeyPart);
                        if (charCode >= 0 && charCode <= 255) {
                            actualChar = static_cast<char>(charCode);
                        } else {
                            std::cerr << "Warning: Invalid char code in config: " << key << std::endl;
                            continue;
                        }
                    }
                    config.charToFreq[actualChar] = std::stof(valueStr);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Warning: Invalid char key format in config: " << key << " (" << ia.what() << ")" << std::endl;
                } catch (const std::out_of_range& oor) {
                     std::cerr << "Warning: Char code out of range in config: " << key << " (" << oor.what() << ")" << std::endl;
                }
            }
            // Add more config keys as needed
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Warning: Invalid value for key '" << key << "' in config: " << valueStr << " (" << ia.what() << ")" << std::endl;
        } catch (const std::out_of_range& oor) {
            std::cerr << "Warning: Value out of range for key '" << key << "' in config: " << valueStr << " (" << oor.what() << ")" << std::endl;
        }
    }
    configFile.close();

    if (config.charToFreq.empty()) {
        std::cerr << "Warning: No character frequencies (CHAR_X) were loaded from the config file." << std::endl;
        std::cerr << "         The program may not function correctly for encoding/decoding text." << std::endl;
    }

    return config;
}