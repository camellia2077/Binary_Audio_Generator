#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

// 包含JSON解析库。
// 请确保json.hpp在你的包含路径中或与源文件在同一目录。
#include "json.hpp" // 如果库安装方式不同，可能是 <nlohmann/json.hpp>
using json = nlohmann::json; // 使用类型别名简化nlohmann::json的使用

using namespace std; // 使用标准命名空间

// --- 音频参数 (Audio Parameters) ---
uint32_t SAMPLE_RATE = 44100;
uint16_t BITS_PER_SAMPLE = 16;
uint16_t NUM_CHANNELS = 1;
double AMPLITUDE = 30000.0;
double FREQUENCY = 880.0;         // 普通哔哔声频率 (Hz)
double END_SIGNAL_FREQUENCY = 440.0; // 新增: 结束音频率 (Hz) - 默认值

// --- 哔哔声和静音持续时间 (Beep and Silence Durations) ---
double SHORT_BEEP_DURATION_MS = 100.0;
double LONG_BEEP_DURATION_MS = 100.0;
double BIT_SILENCE_DURATION_MS = 50.0;
double BYTE_SILENCE_DURATION_MS = 200.0;
double END_SIGNAL_BEEP_DURATION_MS = 300.0; // 新增: 结束音持续时间 (ms) - 默认值


/**
 * @brief 存储应用程序参数和文件路径的结构体。
 */
struct AppArguments {
    string inputFilePath;    // 输入文本文件的路径
    string outputFilePath;   // 生成的WAV音频文件的路径
    string configFilePath;   // JSON配置文件的路径
};

/**
 * @brief 从JSON文件加载配置参数。
 *
 * @param configFilePath 配置文件的路径 (string)。
 * @details 此函数尝试打开并解析指定的JSON配置文件。
 * 如果文件无法打开或解析出错，将输出警告信息，并使用默认参数值。
 * 成功解析后，会更新全局的音频参数和持续时间参数。
 * @note 此函数不返回值，直接修改全局变量。
 */
void loadConfiguration(const string& configFilePath) {
    ifstream configFile(configFilePath);
    if (!configFile.is_open()) {
        cerr << "Warning: Unable to open configuration file '" << configFilePath << "'. Using default settings." << endl;
        return;
    }

    try {
        json configJson;
        configFile >> configJson;
        configFile.close();

        cout << "Successfully parsed configuration file: " << configFilePath << endl;

        if (configJson.contains("audio_parameters")) {
            const auto& audioParams = configJson["audio_parameters"];
            if (audioParams.contains("sample_rate") && audioParams["sample_rate"].is_number_integer())
                SAMPLE_RATE = audioParams["sample_rate"].get<uint32_t>();
            if (audioParams.contains("bits_per_sample") && audioParams["bits_per_sample"].is_number_integer())
                BITS_PER_SAMPLE = audioParams["bits_per_sample"].get<uint16_t>();
            if (audioParams.contains("num_channels") && audioParams["num_channels"].is_number_integer())
                NUM_CHANNELS = audioParams["num_channels"].get<uint16_t>();
            if (audioParams.contains("amplitude") && audioParams["amplitude"].is_number())
                AMPLITUDE = audioParams["amplitude"].get<double>();
            if (audioParams.contains("frequency") && audioParams["frequency"].is_number())
                FREQUENCY = audioParams["frequency"].get<double>();
            // 新增: 加载结束音频率
            if (audioParams.contains("end_signal_frequency") && audioParams["end_signal_frequency"].is_number())
                END_SIGNAL_FREQUENCY = audioParams["end_signal_frequency"].get<double>();
        }

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
            // 新增: 加载结束音持续时间
            if (durations.contains("end_signal_beep") && durations["end_signal_beep"].is_number())
                END_SIGNAL_BEEP_DURATION_MS = durations["end_signal_beep"].get<double>();
        }
    } catch (json::parse_error& e) {
        cerr << "Warning: Configuration file '" << configFilePath << "' JSON parsing error: " << e.what() << ". Affected parameters will use default settings." << endl;
    } catch (json::type_error& e) {
        cerr << "Warning: Configuration file '" << configFilePath << "' JSON type error: " << e.what() << ". Affected parameters will use default settings." << endl;
    } catch (std::exception& e) {
        cerr << "Warning: Unknown error while processing configuration file '" << configFilePath << "': " << e.what() << ". Using default settings." << endl;
    }
}


// --- WAV 文件辅助函数 (WAV File Helper Functions) ---

template <typename T>
void write_little_endian(ofstream& file, T value) {
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

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
    write_little_endian<uint16_t>(file, 1);
    write_little_endian(file, NUM_CHANNELS);
    write_little_endian(file, SAMPLE_RATE);
    write_little_endian(file, byteRate);
    write_little_endian(file, blockAlign);
    write_little_endian(file, BITS_PER_SAMPLE);
    file.write("data", 4);
    write_little_endian(file, dataChunkSize);
}

// --- 音频生成函数 (Audio Generation Functions) ---

/**
 * @brief 生成指定持续时间和频率的哔哔声音频样本。
 *
 * @param duration_ms 哔哔声的持续时间 (毫秒)。
 * @param frequency_hz 哔哔声的频率 (Hz)。
 * @return vector<int16_t> 包含生成音频样本的向量。
 * @details 音频样本是根据传入的频率, 全局 AMPLITUDE (振幅),
 * 和全局 SAMPLE_RATE (采样率) 生成的正弦波。
 * 每个样本是16位有符号整数。
 */
vector<int16_t> generateBeep(double duration_ms, double frequency_hz) { // 修改：添加 frequency_hz 参数
    uint32_t numSamples = static_cast<uint32_t>(SAMPLE_RATE * duration_ms / 1000.0);
    vector<int16_t> samples(numSamples);
    double angleIncrement = 2.0 * M_PI * frequency_hz / SAMPLE_RATE; // 修改：使用传入的 frequency_hz
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

// 为了保持向后兼容性或在其他地方继续使用全局 FREQUENCY，可以保留一个单参数版本
// 或者在使用全局频率的地方显式传入 FREQUENCY
vector<int16_t> generateBeep(double duration_ms) {
    return generateBeep(duration_ms, FREQUENCY); // 调用双参数版本，使用全局 FREQUENCY
}


vector<int16_t> generateSilence(double duration_ms) {
    uint32_t numSamples = static_cast<uint32_t>(SAMPLE_RATE * duration_ms / 1000.0);
    return vector<int16_t>(numSamples, 0);
}

// --- 新的重构函数 (New Refactored Functions) ---

bool initializeApplication(int argc, char* argv[], AppArguments& args) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_txt_file_path>" << endl;
        cerr << "The program will automatically look for 'audio_generator_config.json' in the current directory to override default settings." << endl;
        return false;
    }

    args.inputFilePath = argv[1];
    args.configFilePath = "audio_generator_config.json";

    size_t lastSlash = args.inputFilePath.find_last_of("/\\");
    string inputFileNameBase = (lastSlash == string::npos) ? args.inputFilePath : args.inputFilePath.substr(lastSlash + 1);
    size_t lastDot = inputFileNameBase.find_last_of('.');
    if (lastDot != string::npos) {
        inputFileNameBase = inputFileNameBase.substr(0, lastDot);
    }
    args.outputFilePath = inputFileNameBase + "_audio.wav";

    return true;
}

bool processInputFile(const string& inputFilePath, vector<int16_t>& allSamples) {
    ifstream inputFile(inputFilePath);
    if (!inputFile.is_open()) {
        cerr << "Error: Unable to open input file '" << inputFilePath << "'" << endl;
        return false;
    }

    cout << "Processing binary data from '" << inputFilePath << "' and generating audio samples..." << endl;
    allSamples.clear();
    char character;
    bool firstBit = true;

    while (inputFile.get(character)) {
        vector<int16_t> currentSamples;
        vector<int16_t> silenceSamples;

        if (character == '0') {
            currentSamples = generateBeep(SHORT_BEEP_DURATION_MS); // 使用全局 FREQUENCY
            if (BIT_SILENCE_DURATION_MS > 0) silenceSamples = generateSilence(BIT_SILENCE_DURATION_MS);
            firstBit = false;
        } else if (character == '1') {
            currentSamples = generateBeep(LONG_BEEP_DURATION_MS);  // 使用全局 FREQUENCY
            if (BIT_SILENCE_DURATION_MS > 0) silenceSamples = generateSilence(BIT_SILENCE_DURATION_MS);
            firstBit = false;
        } else if (character == ' ' && !firstBit) {
            if (!allSamples.empty() && BIT_SILENCE_DURATION_MS > 0) {
                uint32_t samples_to_remove = static_cast<uint32_t>(SAMPLE_RATE * BIT_SILENCE_DURATION_MS / 1000.0);
                if (allSamples.size() >= samples_to_remove) {
                    bool was_bit_silence = true;
                    for (size_t i = 0; i < samples_to_remove; ++i) {
                        if (allSamples[allSamples.size() - 1 - i] != 0) {
                            was_bit_silence = false;
                            break;
                        }
                    }
                    if (was_bit_silence) {
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
    return true;
}

bool writeWavOutputFile(const string& outputFilePath, const vector<int16_t>& allSamples) {
    if (allSamples.empty() && END_SIGNAL_BEEP_DURATION_MS <=0) { // 修改: 如果结束音也没有，才不生成
        cout << "Input did not produce any audio samples, and no end signal is configured. No audio file generated." << endl;
        return true;
    }

    cout << "Writing WAV file: " << outputFilePath << endl;
    ofstream outputFile(outputFilePath, ios::binary);
    if (!outputFile.is_open()) {
        cerr << "Error: Unable to create or open output file '" << outputFilePath << "'" << endl;
        return false;
    }

    writeWavHeader(outputFile, static_cast<uint32_t>(allSamples.size()));
    outputFile.write(reinterpret_cast<const char*>(allSamples.data()), allSamples.size() * sizeof(int16_t));
    outputFile.close();

    if (outputFile.good()) {
        cout << "WAV file '" << outputFilePath << "' generated successfully!" << endl;
        return true;
    } else {
        cerr << "Error: An error occurred while writing WAV file '" << outputFilePath << "'." << endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    AppArguments appArgs;

    if (!initializeApplication(argc, argv, appArgs)) {
        return 1;
    }

    cout << "Input file: " << appArgs.inputFilePath << endl;
    cout << "Output file will be: " << appArgs.outputFilePath << endl;
    cout << "Configuration file: " << appArgs.configFilePath << endl;

    loadConfiguration(appArgs.configFilePath);

    vector<int16_t> allSamples;
    auto startTime = chrono::high_resolution_clock::now();

    if (!processInputFile(appArgs.inputFilePath, allSamples)) {
        return 1;
    }

    // --- 新增：在此处添加结束音 ---
    if (END_SIGNAL_BEEP_DURATION_MS > 0) {
        cout << "Generating end signal..." << endl;
        // 如果之前有比特静音，并且希望结束音紧随最后一个数据音，可以考虑移除最后的比特静音
        // 这里为了简单，我们直接在所有内容之后添加，也可以在 processInputFile 的末尾处理
        if (!allSamples.empty() && BIT_SILENCE_DURATION_MS > 0 && BYTE_SILENCE_DURATION_MS == 0) { // 假设如果最后一个是空格（字节间隔），则不移除
            // 检查并移除最后一个比特静音的逻辑可以放在这里，类似于 processInputFile 中处理空格的逻辑
            // 但要注意，如果最后一个字符是空格，那么其后并没有比特静音。
            // 一个更稳妥的方法是，确保在添加结束音前，根据需要添加或移除适当的静音。
            // 为简单起见，我们先假设结束音前不需要移除静音，或者在 processInputFile 的末尾已经处理好了。
            // 或者，我们可以在这里主动添加一个短暂停顿（如果需要的话）
            // allSamples.insert(allSamples.end(), generateSilence(BIT_SILENCE_DURATION_MS).begin(), generateSilence(BIT_SILENCE_DURATION_MS).end());
        }

        vector<int16_t> endSignalSamples = generateBeep(END_SIGNAL_BEEP_DURATION_MS, END_SIGNAL_FREQUENCY);
        if (!endSignalSamples.empty()) {
            allSamples.insert(allSamples.end(), endSignalSamples.begin(), endSignalSamples.end());
        }
        cout << "End signal generated and added to the end of the sequence." << endl;
    }
    // --- 结束音添加完毕 ---


    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    cout << "Processing time: " << duration.count() << " milliseconds" << endl;

    if (!writeWavOutputFile(appArgs.outputFilePath, allSamples)) {
        return 1;
    }

    return 0;
}
