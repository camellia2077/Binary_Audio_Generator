// audio_generator.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <sstream>
#include <algorithm> // For std::tolower
#include <cstdlib>   // For exit, EXIT_FAILURE (though ini_parser handles it)

#include "ini_parser.h" // Include your new INI parser header

// --- Audio generation code (M_PI, writeWavHeader, generateTone, generateSilence) ---
#ifndef M_PI // Define M_PI if not already defined (e.g. by <cmath> on some systems)
    #define M_PI 3.14159265358979323846f
#endif

void writeWavHeader(std::ofstream& file, int sampleRate, int bitsPerSample, int numChannels, int numSamples) { //
    file.write("RIFF", 4); //
    int chunkSize = 36 + numSamples * numChannels * bitsPerSample / 8; //
    file.write(reinterpret_cast<const char*>(&chunkSize), 4); //
    file.write("WAVE", 4); //
    file.write("fmt ", 4); //
    int subchunk1Size = 16; //
    file.write(reinterpret_cast<const char*>(&subchunk1Size), 4); //
    short audioFormat = 1; // PCM //
    file.write(reinterpret_cast<const char*>(&audioFormat), 2); //
    short numChannelsShort = numChannels; //
    file.write(reinterpret_cast<const char*>(&numChannelsShort), 2); //
    file.write(reinterpret_cast<const char*>(&sampleRate), 4); //
    int byteRate = sampleRate * numChannels * bitsPerSample / 8; //
    file.write(reinterpret_cast<const char*>(&byteRate), 4); //
    short blockAlign = numChannels * bitsPerSample / 8; //
    file.write(reinterpret_cast<const char*>(&blockAlign), 2); //
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2); //
    file.write("data", 4); //
    int subchunk2Size = numSamples * numChannels * bitsPerSample / 8; //
    file.write(reinterpret_cast<const char*>(&subchunk2Size), 4); //
}

void generateTone(std::vector<short>& samples, float frequency, float duration, float amplitude, int sampleRate) { //
    int numSamples = static_cast<int>(duration * sampleRate); //
    for (int i = 0; i < numSamples; ++i) { //
        float t = static_cast<float>(i) / sampleRate; //
        float value = amplitude * std::sin(2.0f * M_PI * frequency * t); //
        samples.push_back(static_cast<short>(value)); //
    }
}

void generateSilence(std::vector<short>& samples, float duration, int sampleRate) { //
    int numSamples = static_cast<int>(duration * sampleRate); //
    for (int i = 0; i < numSamples; ++i) { //
        samples.push_back(0); //
    }
}
// --- End of audio generation code ---


int main(int argc, char* argv[]) { //
    std::string configFilename_main = "audio_config.ini"; // Default config file name //
    std::string inputTxtFilename; //
    std::string outputWavFilename_main_cli; // Output filename from CLI //

    // --- Parse Command Line Arguments ---
    // Usage: ./audio_generator <input_txt_file> [output_wav_file] [config_ini_file]
    if (argc < 2) { //
        std::cerr << "Usage: " << argv[0] << " <input_txt_file> [output_wav_file] [config_ini_file]" << std::endl; //
        std::cerr << "  input_txt_file: Path to the text file to encode." << std::endl; //
        std::cerr << "  output_wav_file (optional): Path to the output WAV file." << std::endl; //
        std::cerr << "                         Defaults to value in config_ini_file or '" //
                  << Config().outputWavFilename_config << "'." << std::endl; //
        std::cerr << "  config_ini_file (optional): Path to the configuration INI file." << std::endl; //
        std::cerr << "                         Defaults to '" << configFilename_main << "'." << std::endl; //
        return 1; //
    }

    inputTxtFilename = argv[1]; //
    if (argc >= 3) { //
        outputWavFilename_main_cli = argv[2]; //
    }
    if (argc >= 4) { //
        configFilename_main = argv[3]; //
    }


    // --- Load Configuration ---
    // loadIniConfig will now exit if the file isn't found/readable.
    Config config = loadIniConfig(configFilename_main); //

    // Determine final output WAV filename:
    std::string finalOutputWavFilename; //
    if (!outputWavFilename_main_cli.empty()) { //
        finalOutputWavFilename = outputWavFilename_main_cli; // CLI argument overrides everything //
    } else if (!config.outputWavFilename_config.empty()) { //
        finalOutputWavFilename = config.outputWavFilename_config; // Use config file's value //
    } else { //
        // This case should ideally not be hit if Config struct has a sane default,
        // but as a fallback (or if config.outputWavFilename_config was somehow cleared).
        finalOutputWavFilename = "sound_default_fallback.wav"; //
        std::cerr << "Warning: Output WAV filename not specified by CLI or INI. Using fallback: " //
                  << finalOutputWavFilename << std::endl; //
    }


    // --- Read Input Text File ---
    std::ifstream inputFile(inputTxtFilename); //
    if (!inputFile.is_open()) { //
        std::cerr << "Error: Could not open input text file " << inputTxtFilename << std::endl; //
        return 1; //
    }

    std::stringstream buffer; //
    buffer << inputFile.rdbuf(); //
    std::string textToEncode = buffer.str(); //
    inputFile.close(); //

    if (textToEncode.empty()) { //
        std::cerr << "Error: Input text file is empty or could not be read." << std::endl; //
        return 1; //
    }
    if (config.charToFreq.empty() && !textToEncode.empty()) { //
         std::cerr << "Error: Character to frequency map is empty (check INI file for CHAR_ entries)." //
                   << " Cannot encode text." << std::endl; //
        return 1; //
    }


    std::vector<short> allSamples; //

    // --- Generate Start Tone ---
    if (config.startToneFreq > 0 && config.syncToneDurationS > 0) { //
        // std::cout << "Encoding: START_TONE -> " << config.startToneFreq << " Hz for " << config.syncToneDurationS << "s" << std::endl; // MODIFIED: Commented out
        generateTone(allSamples, config.startToneFreq, config.syncToneDurationS, config.amplitude, config.sampleRate); //
        generateSilence(allSamples, config.silenceDurationS, config.sampleRate); //
    }


    for (char c : textToEncode) { //
        auto it = config.charToFreq.find(c); //
        if (it != config.charToFreq.end()) { //
            // std::cout << "Encoding: '" << c << "' -> " << it->second << " Hz for " << config.toneDurationS << "s" << std::endl; // MODIFIED: Commented out
            generateTone(allSamples, it->second, config.toneDurationS, config.amplitude, config.sampleRate); //
            generateSilence(allSamples, config.silenceDurationS, config.sampleRate); //
        } else { //
            if (c == '\n' || c == '\r') { //
                 // std::cout << "Encoding: newline -> (extra silence)" << std::endl; // MODIFIED: Commented out
                 generateSilence(allSamples, config.toneDurationS + config.silenceDurationS, config.sampleRate); // Or just a specific silence duration for newlines //
            } else { //
                // std::cerr << "Warning: Character '" << c << "' (ASCII: " << static_cast<int>(static_cast<unsigned char>(c)) // MODIFIED: Commented out
                //           << ") not in frequency map (defined in " << configFilename_main << "). Skipping." << std::endl; // MODIFIED: Commented out
                generateSilence(allSamples, config.toneDurationS + config.silenceDurationS, config.sampleRate); //
            }
        }
    }

    // --- Generate End Tone ---
    if (config.endToneFreq > 0 && config.syncToneDurationS > 0) { //
        // std::cout << "Encoding: END_TONE -> " << config.endToneFreq << " Hz for " << config.syncToneDurationS << "s" << std::endl; // MODIFIED: Commented out
        generateTone(allSamples, config.endToneFreq, config.syncToneDurationS, config.amplitude, config.sampleRate); //
        generateSilence(allSamples, config.silenceDurationS, config.sampleRate); // Add silence after end tone too //
    }


    if (allSamples.empty() && textToEncode.length() > 0) { // Check if text had content but no samples generated //
        std::cerr << "Warning: No audio samples generated, though input text was provided. " //
                  << "This might be due to all characters being unmapped in the INI." << std::endl; //
    }
     if (allSamples.empty() && textToEncode.length() == 0 ) { //
        std::cout << "Input text was empty. No audio data to generate." << std::endl; //
    }


    std::ofstream outFile(finalOutputWavFilename, std::ios::binary); //
    if (!outFile) { //
        std::cerr << "Error: Could not open output file " << finalOutputWavFilename << std::endl; //
        return 1; //
    }

    writeWavHeader(outFile, config.sampleRate, config.bitsPerSample, 1, allSamples.size()); //
    if(!allSamples.empty()){ //
        outFile.write(reinterpret_cast<const char*>(allSamples.data()), allSamples.size() * sizeof(short)); //
    } else { //
        std::cout << "No audio samples to write for " << finalOutputWavFilename << ". An empty WAV file might be created." << std::endl; //
    }

    outFile.close(); //
    std::cout << "Audio generation process complete. Output: " << finalOutputWavFilename << std::endl; //

    return 0; //
}