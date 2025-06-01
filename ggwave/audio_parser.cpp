// audio_parser.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm> // For std::max_element, std::distance
#include <cstdlib>   // For exit, EXIT_FAILURE

#include "ini_parser.h" // Include INI parser header

// Define M_PI if not already defined (e.g. by <cmath> on some systems)
#ifndef M_PI
    #define M_PI 3.14159265358979323846f
#endif

// This map will be populated from the Config struct
std::map<float, char> freqToChar_decoder; //

// Function to initialize the freqToChar_decoder map from the loaded config
void initializeFreqToCharMapFromConfig(const Config& config) { //
    freqToChar_decoder.clear(); //
    for (const auto& pair : config.charToFreq) { //
        freqToChar_decoder[pair.second] = pair.first; //
    }
    if (freqToChar_decoder.empty() && !config.charToFreq.empty()) { //
        std::cerr << "Warning: freqToChar_decoder map is empty after initialization, but charToFreq was not." << std::endl; //
    } else if (config.charToFreq.empty()) { //
        std::cerr << "Warning: charToFreq map from config is empty. Decoder will not be able to map frequencies to characters." << std::endl; //
    }
}


// --- DFT Function (remains mostly the same) ---
float getMagnitudeForFrequency(const std::vector<short>& samples, float targetFreq, int sampleRate) { //
    float realPart = 0.0f; //
    float imagPart = 0.0f; //
    int N = samples.size(); //

    if (N == 0) return 0.0f; //

    for (int n = 0; n < N; ++n) { //
        float t = static_cast<float>(n) / sampleRate; //
        float angle = 2.0f * M_PI * targetFreq * t; //
        realPart += samples[n] * std::cos(angle); //
        imagPart -= samples[n] * std::sin(angle); // Minus due to e^(-j...) //
    }
    return std::sqrt(realPart * realPart + imagPart * imagPart) / N; // Normalize by N //
}

// Modified to use the global freqToChar_decoder map and config for threshold
float detectFrequency(const std::vector<short>& samples, int currentSampleRate, const Config& config, float specificFreqToCheck = 0.0f) { //
    float maxMagnitude = -1.0; //
    float dominantFreq = 0.0f; //

    if (samples.empty()) return 0.0f; //

    if (specificFreqToCheck > 0.0f) { //
        float magnitude = getMagnitudeForFrequency(samples, specificFreqToCheck, currentSampleRate); //
        const float MIN_MAGNITUDE_THRESHOLD = 500; // Arbitrary, should ideally be in Config or adaptive //
        if (magnitude > MIN_MAGNITUDE_THRESHOLD && magnitude > maxMagnitude) { //
            maxMagnitude = magnitude; //
            dominantFreq = specificFreqToCheck; //
        }
    } else if (!freqToChar_decoder.empty()) { //
        for (auto const& [freq_key, val_char] : freqToChar_decoder) { //
            float magnitude = getMagnitudeForFrequency(samples, freq_key, currentSampleRate); //
            const float MIN_MAGNITUDE_THRESHOLD = 500; // Arbitrary //
            if (magnitude > MIN_MAGNITUDE_THRESHOLD && magnitude > maxMagnitude) { //
                maxMagnitude = magnitude; //
                dominantFreq = freq_key; //
            }
        }
    }

    if (dominantFreq == 0.0f) { //
        return 0.0f; // Considered silence or no clear tone //
    }

    return dominantFreq; //
}


// Function to skip WAV header (simplified, assumes valid PCM)
bool skipWavHeader(std::ifstream& file, int& fileSampleRate, short& fileBitsPerSample, int& dataSize) { //
    char buffer[4]; //
    if (!file.read(buffer, 4) || std::string(buffer, 4) != "RIFF") return false; //
    file.seekg(4, std::ios::cur); // Skip chunk size //
    if (!file.read(buffer, 4) || std::string(buffer, 4) != "WAVE") return false; //
    if (!file.read(buffer, 4) || std::string(buffer, 4) != "fmt ") return false; //
    file.seekg(4, std::ios::cur); // Skip subchunk1 size //
    short audioFormat; //
    file.read(reinterpret_cast<char*>(&audioFormat), 2); //
    if (audioFormat != 1) { // Not PCM //
        std::cerr << "Error: WAV file is not in PCM format." << std::endl; //
        return false; //
    }

    short numChannels; //
    file.read(reinterpret_cast<char*>(&numChannels), 2); //
    if (numChannels != 1) { //
        std::cerr << "Warning: WAV file is not mono. This decoder expects mono. Decoding might be inaccurate." << std::endl; //
    }

    file.read(reinterpret_cast<char*>(&fileSampleRate), 4); // Read sample rate //
    file.seekg(4, std::ios::cur); // Skip byte rate //
    file.seekg(2, std::ios::cur); // Skip block align //
    file.read(reinterpret_cast<char*>(&fileBitsPerSample), 2); // Read bits per sample //

    std::string chunkID = ""; //
    while(file.read(buffer, 4)) { //
        chunkID = std::string(buffer, 4); //
        if (!file.read(reinterpret_cast<char*>(&dataSize), 4)) { // Size of this chunk //
            std::cerr << "Error: Could not read chunk size after ID '" << chunkID << "'." << std::endl; //
            return false; //
        }
        if (chunkID == "data") { //
            return true; // Found data chunk //
        }
        file.seekg(dataSize, std::ios::cur); //
        if (file.fail() || file.eof()) { //
             std::cerr << "Error: Failed seeking past chunk or EOF reached while searching for 'data' chunk." << std::endl; //
             return false; //
        }
    }
    std::cerr << "Error: 'data' chunk not found in WAV file." << std::endl; //
    return false; // data chunk not found //
}


int main(int argc, char* argv[]) { //
    std::string configFilename_decoder = "audio_config.ini"; // Default config file //
    std::string inputWavFilename; //

    if (argc < 2) { //
        std::cerr << "Usage: " << argv[0] << " <input_wav_file> [config_ini_file]" << std::endl; //
        std::cerr << "  input_wav_file: Path to the WAV file to decode." << std::endl; //
        std::cerr << "  config_ini_file (optional): Path to the configuration INI file." << std::endl; //
        std::cerr << "                         Defaults to '" << configFilename_decoder << "'." << std::endl; //
        return 1; //
    }

    inputWavFilename = argv[1]; //
    if (argc >= 3) { //
        configFilename_decoder = argv[2]; //
    }

    Config config = loadIniConfig(configFilename_decoder); //

    initializeFreqToCharMapFromConfig(config); //
    if (freqToChar_decoder.empty()) { //
        std::cerr << "Error: Frequency to character map is empty. Cannot decode. Check CHAR_ entries in " //
                  << configFilename_decoder << "." << std::endl; //
        return 1; //
    }


    std::ifstream inFile(inputWavFilename, std::ios::binary); //
    if (!inFile) { //
        std::cerr << "Error: Could not open input WAV file " << inputWavFilename << std::endl; //
        return 1; //
    }

    int fileSampleRate; //
    short fileBitsPerSample; //
    int dataChunkSize; //

    if (!skipWavHeader(inFile, fileSampleRate, fileBitsPerSample, dataChunkSize)) { //
        std::cerr << "Error: Invalid or unsupported WAV file format." << std::endl; //
        inFile.close(); //
        return 1; //
    }

    if (fileSampleRate != config.sampleRate) { //
        std::cerr << "Warning: WAV file sample rate (" << fileSampleRate //
                  << ") differs from config's expected rate (" << config.sampleRate //
                  << "). Results may be inaccurate." << std::endl; //
    }
    if (fileBitsPerSample != config.bitsPerSample) { //
         std::cerr << "Warning: Decoder expects " << config.bitsPerSample //
                   << "-bit audio (from config), but WAV file is " << fileBitsPerSample << "-bit." << std::endl; //
    }
    if (fileBitsPerSample != 16) { // Explicit check for 16-bit assumption //
        std::cerr << "Error: This decoder currently only supports 16-bit audio samples from WAV." << std::endl; //
        inFile.close(); //
        return 1; //
    }


    std::string decodedText = ""; //
    int currentProcessingSampleRate = fileSampleRate; //

    int samplesPerDataTone = static_cast<int>(config.toneDurationS * currentProcessingSampleRate); //
    int samplesPerSyncTone = static_cast<int>(config.syncToneDurationS * currentProcessingSampleRate); //
    int samplesPerSilence = static_cast<int>(config.silenceDurationS * currentProcessingSampleRate); //


    std::vector<short> audioBuffer(dataChunkSize / (fileBitsPerSample / 8)); //
    inFile.read(reinterpret_cast<char*>(audioBuffer.data()), dataChunkSize); //
    if (static_cast<size_t>(inFile.gcount()) != static_cast<size_t>(dataChunkSize)) { //
        std::cerr << "Warning: Could not read the full audio data chunk. Read " //
                  << inFile.gcount() << " bytes, expected " << dataChunkSize << "." << std::endl; //
        if(inFile.gcount() == 0 && dataChunkSize > 0) { //
            std::cerr << "Error: No data read from audio buffer." << std::endl; //
            inFile.close(); //
            return 1; //
        }
        audioBuffer.resize(inFile.gcount() / (fileBitsPerSample / 8)); //
    }
    inFile.close(); //

    if (audioBuffer.empty()) { //
        std::cerr << "Error: Audio buffer is empty after reading WAV file. Cannot decode." << std::endl; //
        return 1; //
    }


    int currentPos = 0; //
    bool startToneDetected = false; //

    // 1. Detect Start Tone
    if (config.startToneFreq > 0 && config.syncToneDurationS > 0) { //
        if (currentPos + samplesPerSyncTone <= static_cast<int>(audioBuffer.size())) { //
            std::vector<short> segment(audioBuffer.begin() + currentPos, audioBuffer.begin() + currentPos + samplesPerSyncTone); //
            float detectedFreq = detectFrequency(segment, currentProcessingSampleRate, config, config.startToneFreq); //
            if (std::abs(detectedFreq - config.startToneFreq) < config.freqTolerance) { //
                // std::cout << "Detected START_TONE: " << detectedFreq << " Hz (Expected: " << config.startToneFreq << " Hz)" << std::endl; // MODIFIED: Commented out
                startToneDetected = true; //
                currentPos += (samplesPerSyncTone + samplesPerSilence); // Move past start tone and its silence //
            } else { //
                std::cerr << "Warning: START_TONE not detected clearly at the beginning (Detected: " << detectedFreq << " Hz, Expected: " << config.startToneFreq << " Hz)." //
                          << " Proceeding with decoding, but results might be inaccurate." << std::endl; //
            }
        } else { //
            std::cerr << "Warning: Not enough audio data to reliably detect start tone. Attempting to proceed." << std::endl; //
        }
    } else { //
        // std::cout << "Start tone frequency or duration not configured. Skipping start tone detection." << std::endl; // MODIFIED: Commented out
        startToneDetected = true; // Assume we can start decoding data directly //
    }


    // 2. Decode Data Tones until End Tone or end of buffer
    bool endToneFound = false; //
    while (currentPos + samplesPerDataTone <= static_cast<int>(audioBuffer.size()) && !endToneFound) { //
        if (config.endToneFreq > 0 && config.syncToneDurationS > 0) { //
            if (currentPos + samplesPerSyncTone <= static_cast<int>(audioBuffer.size())) { //
                std::vector<short> end_segment_check(audioBuffer.begin() + currentPos, audioBuffer.begin() + currentPos + samplesPerSyncTone); //
                float potentialEndFreq = detectFrequency(end_segment_check, currentProcessingSampleRate, config, config.endToneFreq); //
                if (std::abs(potentialEndFreq - config.endToneFreq) < config.freqTolerance) { //
                    // std::cout << "Detected END_TONE: " << potentialEndFreq << " Hz (Expected: " << config.endToneFreq << " Hz). Stopping decoding data." << std::endl; // MODIFIED: Commented out
                    endToneFound = true; //
                    currentPos += (samplesPerSyncTone + samplesPerSilence); // Consume end tone //
                    break; // End tone found, stop decoding data //
                }
            }
        }

        std::vector<short> data_segment(audioBuffer.begin() + currentPos, audioBuffer.begin() + currentPos + samplesPerDataTone); //
        float detectedDataFreq = detectFrequency(data_segment, currentProcessingSampleRate, config); // Pass full config //

        bool charFoundForFreq = false; //
        if (detectedDataFreq > 0.0f) { //
            for (auto const& [freq_map_key, character] : freqToChar_decoder) { //
                if (std::abs(detectedDataFreq - freq_map_key) < config.freqTolerance) { //
                    decodedText += character; //
                    // std::cout << "Detected Freq: " << detectedDataFreq << " Hz (Matches: " << freq_map_key << " Hz) -> Char: '" << character << "'" << std::endl; // MODIFIED: Commented out
                    charFoundForFreq = true; //
                    break; //
                }
            }
            if (!charFoundForFreq) { //
                 // std::cout << "Detected Freq: " << detectedDataFreq << " Hz -> No matching char in map within tolerance (" << config.freqTolerance << " Hz)." << std::endl; // MODIFIED: Commented out
            }
        } else { //
            // std::cout << "Silence or unclear signal detected in data segment at sample " << currentPos << std::endl; // MODIFIED: Commented out
        }
        currentPos += (samplesPerDataTone + samplesPerSilence); // Move to the start of the next potential tone //
    }

    if (!endToneFound && config.endToneFreq > 0) { //
        std::cout << "Note: Reached end of audio data, or remaining data too short. End tone was not explicitly detected." << std::endl; //
    }


    std::cout << "\n--- Decoded Text ---" << std::endl; //
    if (decodedText.empty()){ //
        std::cout << "(No characters decoded)" << std::endl; //
    } else { //
        std::cout << decodedText << std::endl; //
    }
    std::cout << "--------------------" << std::endl; //

    // MODIFIED: Add functionality to output decoded text to a file
    const std::string decodedOutputFilename = "decode_content.txt";
    std::ofstream outFileStream(decodedOutputFilename);
    if (!outFileStream.is_open()) {
        std::cerr << "Error: Could not open file " << decodedOutputFilename << " for writing decoded text." << std::endl;
        // Continue without writing to file if it fails, or return 1 if this is critical
    } else {
        outFileStream << decodedText;
        outFileStream.close();
        std::cout << "Decoded content also saved to: " << decodedOutputFilename << std::endl;
    }

    return 0; //
}