// ini_parser.h
#ifndef INI_PARSER_H
#define INI_PARSER_H

#include <string>
#include <vector>
#include <map>

// Configuration Struct
struct Config {
    int sampleRate = 44100;
    int bitsPerSample = 16;
    float toneDurationS = 0.2f;
    float amplitude = 0.5f * 32767; // Default amplitude
    float silenceDurationS = 0.05f;

    float startToneFreq = 500.0f;
    float endToneFreq = 4000.0f;
    float syncToneDurationS = 0.3f;

    std::map<char, float> charToFreq;
    std::string outputWavFilename_config = "sound.wav"; // Default output filename from config perspective

    // New field for decoder/parser
    float freqTolerance = 25.0f; // Default frequency tolerance for decoder
};

// Function to load configuration from an INI file
Config loadIniConfig(const std::string& filename);

#endif // INI_PARSER_H
