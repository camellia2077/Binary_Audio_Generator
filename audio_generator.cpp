#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

// Include for JSON parsing.
// Make sure json.hpp is in your include path or same directory.
#include "json.hpp" // Or <nlohmann/json.hpp> if installed differently
using json = nlohmann::json;

using namespace std;

// --- 音频参数 (Audio Parameters) ---
// These are now variables that can be overridden by JSON configuration.
// They are initialized with default values.
uint32_t SAMPLE_RATE = 44100;      // 采样率 (Hz) - Samples per second
uint16_t BITS_PER_SAMPLE = 16;     // 位深度 - Bits per audio sample
uint16_t NUM_CHANNELS = 1;         // 声道数 (单声道) - Number of channels (Mono)
double AMPLITUDE = 30000.0;        // 振幅 (最大 32767 for 16-bit) - Amplitude (Max 32767 for 16-bit)
double FREQUENCY = 880.0;          // 哔哔声频率 (Hz) - Frequency of the beep (Hz)

// --- 哔哔声和静音持续时间 (Beep and Silence Durations) ---
double SHORT_BEEP_DURATION_MS = 100.0; // 短音 (0) 持续时间 (毫秒) - Short beep (0) duration (ms)
double LONG_BEEP_DURATION_MS = 100.0;  // 长音 (1) 持续时间 (毫秒) - Long beep (1) duration (ms)
double BIT_SILENCE_DURATION_MS = 50.0; // 比特之间的静音 (毫秒) - Silence between bits (ms)
double BYTE_SILENCE_DURATION_MS = 200.0;// 字节 (空格) 之间的静音 (毫秒) - Silence between bytes (space) (ms)

// Function to load configuration from JSON file
void loadConfiguration(const string& configFilePath) {
    ifstream configFile(configFilePath);
    if (!configFile.is_open()) {
        cerr << "Warning: Could not open configuration file '" << configFilePath << "'. Using default settings." << endl;
        return;
    }

    try {
        json configJson;
        configFile >> configJson;
        configFile.close();

        cout << "Successfully parsed configuration file: " << configFilePath << endl;

        // 音频参数 (Audio Parameters)
        if (configJson.contains("audio_parameters")) {
            const auto& audioParams = configJson["audio_parameters"];
            if (audioParams.contains("sample_rate") && audioParams["sample_rate"].is_number_integer())
                SAMPLE_RATE = audioParams["sample_rate"].get<uint32_t>();
            if (audioParams.contains("bits_per_sample") && audioParams["bits_per_sample"].is_number_integer())
                BITS_PER_SAMPLE = audioParams["bits_per_sample"].get<uint16_t>();
            if (audioParams.contains("num_channels") && audioParams["num_channels"].is_number_integer())
                NUM_CHANNELS = audioParams["num_channels"].get<uint16_t>();
            if (audioParams.contains("amplitude") && audioParams["amplitude"].is_number()) // .is_number() for float/double
                AMPLITUDE = audioParams["amplitude"].get<double>();
            if (audioParams.contains("frequency") && audioParams["frequency"].is_number())
                FREQUENCY = audioParams["frequency"].get<double>();
        }

        // 哔哔声和静音持续时间 (Beep and Silence Durations)
        if (configJson.contains("durations_ms")) {
            const auto& durations = configJson["durations_ms"];
            if (durations.contains("short_beep") && durations["short_beep"].is_number())
                SHORT_BEEP_DURATION_MS = durations["short_beep"].get<double>();
            if (durations.contains("long_beep") && durations["long_beep"].is_number())
                LONG_BEEP_DURATION_MS = durations["long_beep"].get<double>();
            if (durations.contains("bit_silence") && durations["bit_silence"].is_number())
                BIT_SILENCE_DURATION_MS = durations["bit_silence"].get<double>();
            if (durations.contains("byte_silence") && durations["byte_silence"].is_number())
                BYTE_SILENCE_DURATION_MS = durations["byte_silence"].get<double>();
        }

        cout << "Configuration loaded successfully:" << endl;
        cout << "  SAMPLE_RATE: " << SAMPLE_RATE << " Hz" << endl;
        cout << "  BITS_PER_SAMPLE: " << BITS_PER_SAMPLE << endl;
        cout << "  NUM_CHANNELS: " << NUM_CHANNELS << endl;
        cout << "  AMPLITUDE: " << AMPLITUDE << endl;
        cout << "  FREQUENCY: " << FREQUENCY << " Hz" << endl;
        cout << "  SHORT_BEEP_DURATION_MS: " << SHORT_BEEP_DURATION_MS << " ms" << endl;
        cout << "  LONG_BEEP_DURATION_MS: " << LONG_BEEP_DURATION_MS << " ms" << endl;
        cout << "  BIT_SILENCE_DURATION_MS: " << BIT_SILENCE_DURATION_MS << " ms" << endl;
        cout << "  BYTE_SILENCE_DURATION_MS: " << BYTE_SILENCE_DURATION_MS << " ms" << endl;

    } catch (json::parse_error& e) {
        cerr << "Warning: JSON parsing error in '" << configFilePath << "': " << e.what() << ". Using default settings for affected parameters." << endl;
    } catch (json::type_error& e) {
        cerr << "Warning: JSON type error in '" << configFilePath << "': " << e.what() << ". Using default settings for affected parameters." << endl;
    } catch (std::exception& e) {
        cerr << "Warning: An unexpected error occurred while processing '" << configFilePath << "': " << e.what() << ". Using default settings." << endl;
    }
}


// --- WAV 文件辅助函数 (WAV File Helper Functions) ---

// 写入小端序整数 (Write little-endian integer)
template <typename T>
void write_little_endian(ofstream& file, T value) {
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

// 生成 WAV 文件头 (Generate WAV File Header)
void writeWavHeader(ofstream& file, uint32_t totalAudioSamples) {
    uint32_t dataChunkSize = totalAudioSamples * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    uint32_t riffChunkSize = 36 + dataChunkSize;
    uint16_t blockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    uint32_t byteRate = SAMPLE_RATE * blockAlign;

    file.write("RIFF", 4);
    write_little_endian(file, riffChunkSize);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    write_little_endian<uint32_t>(file, 16);
    write_little_endian<uint16_t>(file, 1); // PCM
    write_little_endian(file, NUM_CHANNELS);
    write_little_endian(file, SAMPLE_RATE);
    write_little_endian(file, byteRate);
    write_little_endian(file, blockAlign);
    write_little_endian(file, BITS_PER_SAMPLE);
    file.write("data", 4);
    write_little_endian(file, dataChunkSize);
}

// --- 音频生成函数 (Audio Generation Functions) ---

// 生成哔哔声样本 (Generate beep samples)
vector<int16_t> generateBeep(double duration_ms) {
    uint32_t numSamples = static_cast<uint32_t>(SAMPLE_RATE * duration_ms / 1000.0);
    vector<int16_t> samples(numSamples);
    double angleIncrement = 2.0 * M_PI * FREQUENCY / SAMPLE_RATE;
    double currentAngle = 0.0;

    for (uint32_t i = 0; i < numSamples; ++i) {
        double sampleValue = AMPLITUDE * sin(currentAngle);
        samples[i] = static_cast<int16_t>(sampleValue);
        currentAngle += angleIncrement;
        if (currentAngle > 2.0 * M_PI) {
            currentAngle -= 2.0 * M_PI;
        }
    }
    return samples;
}

// 生成静音样本 (Generate silence samples)
vector<int16_t> generateSilence(double duration_ms) {
    uint32_t numSamples = static_cast<uint32_t>(SAMPLE_RATE * duration_ms / 1000.0);
    return vector<int16_t>(numSamples, 0);
}

int main(int argc, char* argv[]) {

    // MODIFIED: Updated usage message and argument check
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_txt_file_path>" << endl;
        cerr << "The program will automatically look for 'audio_generator_config.json' in the current directory to override default settings." << endl;
        return 1;
    }

    string inputFilePath = argv[1];
    // configuration file name 
    string configFilePath = "audio_generator_config.json";

    // MODIFIED: Always attempt to load the fixed configuration file.
    // The loadConfiguration function will print messages about its status (e.g., file not found, successfully loaded, or errors).
    cout << "Attempting to load configuration from: " << configFilePath << endl;
    loadConfiguration(configFilePath);
    // The old 'else' block for "No configuration file provided" is removed,
    // as we now always attempt to load audio_generator_config.json.
    // The loadConfiguration function handles the messaging if the file is not found or has errors.

    string outputFilePath = "output_audio.wav";
    size_t lastSlash = inputFilePath.find_last_of("/\\");
    string inputFileNameBase = (lastSlash == string::npos) ? inputFilePath : inputFilePath.substr(lastSlash + 1);
    size_t lastDot = inputFileNameBase.find_last_of('.');
    if (lastDot != string::npos) {
        inputFileNameBase = inputFileNameBase.substr(0, lastDot);
    }
    outputFilePath = inputFileNameBase + "_audio.wav";


    cout << "Reading input file: " << inputFilePath << endl;

    ifstream inputFile(inputFilePath);
    if (!inputFile.is_open()) {
        cerr << "Error: Could not open input file '" << inputFilePath << "'" << endl;
        return 1;
    }

    vector<int16_t> allSamples;
    char character;
    bool firstBit = true;


    cout << "Processing binary data and generating audio samples..." << endl;

    auto startTime = chrono::high_resolution_clock::now();

    while (inputFile.get(character)) {
        vector<int16_t> currentSamples;
        vector<int16_t> silenceSamples;

        if (character == '0') {
            currentSamples = generateBeep(SHORT_BEEP_DURATION_MS);
            if (BIT_SILENCE_DURATION_MS > 0) silenceSamples = generateSilence(BIT_SILENCE_DURATION_MS);
            firstBit = false;
        } else if (character == '1') {
            currentSamples = generateBeep(LONG_BEEP_DURATION_MS);
             if (BIT_SILENCE_DURATION_MS > 0) silenceSamples = generateSilence(BIT_SILENCE_DURATION_MS);
            firstBit = false;
        } else if (character == ' ' && !firstBit) {
            if (!allSamples.empty() && BIT_SILENCE_DURATION_MS > 0) {
                 uint32_t samples_to_remove = static_cast<uint32_t>(SAMPLE_RATE * BIT_SILENCE_DURATION_MS / 1000.0);
                 if (allSamples.size() >= samples_to_remove) {
                    bool was_bit_silence = true;
                    for(size_t i = 0; i < samples_to_remove; ++i) {
                        if (allSamples[allSamples.size() - 1 - i] != 0) {
                            was_bit_silence = false;
                            break;
                        }
                    }
                    if(was_bit_silence) {
                        allSamples.resize(allSamples.size() - samples_to_remove);
                    }
                 }
            }
            if (BYTE_SILENCE_DURATION_MS > 0) currentSamples = generateSilence(BYTE_SILENCE_DURATION_MS);
            silenceSamples.clear();
            firstBit = true;
        } else if (character == '\n' || character == '\r') {
            continue;
        } else {
            cerr << "Warning: Encountered unexpected character '" << character << "' (ASCII: " << static_cast<int>(character) << ") in input file. Ignoring." << endl;
            continue;
        }

        if (!currentSamples.empty()) {
            allSamples.insert(allSamples.end(), currentSamples.begin(), currentSamples.end());
        }
        if (!silenceSamples.empty()) {
            allSamples.insert(allSamples.end(), silenceSamples.begin(), silenceSamples.end());
        }
    }

    inputFile.close();

    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);

    cout << "Text-to-sound processing complete." << endl;
    cout << "Processing time: " << duration.count() << " milliseconds" << endl;

    if (allSamples.empty()) {
        cout << "Input file is empty or contains no valid '0' or '1'. No audio file generated." << endl;
        return 0;
    }

    cout << "Writing WAV file: " << outputFilePath << endl;

    ofstream outputFile(outputFilePath, ios::binary);
    if (!outputFile.is_open()) {
        cerr << "Error: Could not create or open output file '" << outputFilePath << "'" << endl;
        return 1;
    }

    writeWavHeader(outputFile, static_cast<uint32_t>(allSamples.size()));

    outputFile.write(reinterpret_cast<const char*>(allSamples.data()),
                     allSamples.size() * sizeof(int16_t));

    outputFile.close();

    if (outputFile.good()) {
        cout << "WAV file '" << outputFilePath << "' generated successfully!" << endl;
    } else {
        cerr << "Error: An error occurred while writing the WAV file." << endl;
        return 1;
    }

    return 0;
}
