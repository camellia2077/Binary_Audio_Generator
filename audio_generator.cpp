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
// 这些现在是可以被JSON配置文件覆盖的变量。
// 它们被初始化为默认值。
uint32_t SAMPLE_RATE = 44100;      // 采样率 (Hz) - 每秒采样点数
uint16_t BITS_PER_SAMPLE = 16;     // 位深度 - 每个音频样本的位数
uint16_t NUM_CHANNELS = 1;         // 声道数 (单声道)
double AMPLITUDE = 30000.0;        // 振幅 (16位音频最大为 32767)
double FREQUENCY = 880.0;          // 哔哔声频率 (Hz)

// --- 哔哔声和静音持续时间 (Beep and Silence Durations) ---
double SHORT_BEEP_DURATION_MS = 100.0; // 短音 ('0') 持续时间 (毫秒)
double LONG_BEEP_DURATION_MS = 100.0;  // 长音 ('1') 持续时间 (毫秒)
double BIT_SILENCE_DURATION_MS = 50.0; // 比特之间的静音持续时间 (毫秒)
double BYTE_SILENCE_DURATION_MS = 200.0;// 字节 (空格) 之间的静音持续时间 (毫秒)

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
    ifstream configFile(configFilePath); // 尝试打开配置文件
    if (!configFile.is_open()) { // 如果文件打开失败
        cerr << "警告: 无法打开配置文件 '" << configFilePath << "'。将使用默认设置。" << endl;
        return; // 退出函数
    }

    try {
        json configJson; // 创建json对象
        configFile >> configJson; // 从文件流读取JSON数据到json对象
        configFile.close(); // 关闭文件

        cout << "成功解析配置文件: " << configFilePath << endl;

        // 更新音频参数
        if (configJson.contains("audio_parameters")) { // 检查是否存在"audio_parameters"键
            const auto& audioParams = configJson["audio_parameters"]; // 获取音频参数对象
            // 逐个检查并更新音频参数
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
        }

        // 更新哔哔声和静音持续时间参数
        if (configJson.contains("durations_ms")) { // 检查是否存在"durations_ms"键
            const auto& durations = configJson["durations_ms"]; // 获取持续时间参数对象
            // 逐个检查并更新持续时间参数
            if (durations.contains("short_beep") && durations["short_beep"].is_number())
                SHORT_BEEP_DURATION_MS = durations["short_beep"].get<double>();
            if (durations.contains("long_beep") && durations["long_beep"].is_number())
                LONG_BEEP_DURATION_MS = durations["long_beep"].get<double>();
            if (durations.contains("bit_silence") && durations["bit_silence"].is_number())
                BIT_SILENCE_DURATION_MS = durations["bit_silence"].get<double>();
            if (durations.contains("byte_silence") && durations["byte_silence"].is_number())
                BYTE_SILENCE_DURATION_MS = durations["byte_silence"].get<double>();
        }
    } catch (json::parse_error& e) { // 捕获JSON解析错误
        cerr << "警告: 配置文件 '" << configFilePath << "' JSON解析错误: " << e.what() << "。受影响的参数将使用默认设置。" << endl;
    } catch (json::type_error& e) { // 捕获JSON类型错误
        cerr << "警告: 配置文件 '" << configFilePath << "' JSON类型错误: " << e.what() << "。受影响的参数将使用默认设置。" << endl;
    } catch (std::exception& e) { // 捕获其他标准异常
        cerr << "警告: 处理配置文件 '" << configFilePath << "' 时发生未知错误: " << e.what() << "。将使用默认设置。" << endl;
    }
}


// --- WAV 文件辅助函数 (WAV File Helper Functions) ---

/**
 * @brief 将一个值以小端序格式写入文件流。
 *
 * @tparam T 值的类型 (例如 int, uint16_t, uint32_t)。
 * @param file 输出文件流 (ofstream)。
 * @param value 要写入的值。
 * @details WAV文件格式要求多字节数值以小端序存储。
 */
template <typename T>
void write_little_endian(ofstream& file, T value) {
    // reinterpret_cast用于将value的地址转换为char*类型，以便按字节写入
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

/**
 * @brief 向文件流写入WAV文件的头部信息。
 *
 * @param file 输出文件流 (ofstream)，文件必须以二进制模式打开。
 * @param totalAudioSamples 音频数据部分的总样本数 (不包括声道因素)。
 * @details 此函数根据全局音频参数 (SAMPLE_RATE, NUM_CHANNELS, BITS_PER_SAMPLE)
 * 和传入的总样本数来构建并写入标准的WAV文件头。
 */
void writeWavHeader(ofstream& file, uint32_t totalAudioSamples) {
    // 计算WAV头中的各个字段值
    uint32_t dataChunkSize = totalAudioSamples * NUM_CHANNELS * (BITS_PER_SAMPLE / 8); // 数据块大小
    uint32_t riffChunkSize = 36 + dataChunkSize; // RIFF块总大小 (文件总大小 - 8)
    uint16_t blockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8); // 数据块对齐单位 (每个采样帧的字节数)
    uint32_t byteRate = SAMPLE_RATE * blockAlign; // 每秒字节数

    // 写入RIFF头
    file.write("RIFF", 4);                            // "RIFF"标记
    write_little_endian(file, riffChunkSize);         // RIFF块大小
    file.write("WAVE", 4);                            // "WAVE"标记

    // 写入fmt子块
    file.write("fmt ", 4);                            // "fmt "标记 (注意末尾有空格)
    write_little_endian<uint32_t>(file, 16);          // fmt子块的大小 (对于PCM通常是16)
    write_little_endian<uint16_t>(file, 1);           // 音频格式 (1表示PCM线性采样)
    write_little_endian(file, NUM_CHANNELS);          //声道数
    write_little_endian(file, SAMPLE_RATE);           // 采样率
    write_little_endian(file, byteRate);              // 每秒字节数 (传输速率)
    write_little_endian(file, blockAlign);            // 数据块对齐
    write_little_endian(file, BITS_PER_SAMPLE);       // 每个样本的位数

    // 写入data子块标记和大小
    file.write("data", 4);                            // "data"标记
    write_little_endian(file, dataChunkSize);         // 数据部分的大小
}

// --- 音频生成函数 (Audio Generation Functions) ---

/**
 * @brief 生成指定持续时间的哔哔声音频样本。
 *
 * @param duration_ms 哔哔声的持续时间 (毫秒)。
 * @return vector<int16_t> 包含生成音频样本的向量。
 * @details 音频样本是根据全局参数 FREQUENCY (频率), AMPLITUDE (振幅),
 * 和 SAMPLE_RATE (采样率) 生成的正弦波。
 * 每个样本是16位有符号整数。
 */
vector<int16_t> generateBeep(double duration_ms) {
    // 计算总样本数
    uint32_t numSamples = static_cast<uint32_t>(SAMPLE_RATE * duration_ms / 1000.0);
    vector<int16_t> samples(numSamples); // 创建存储样本的向量
    // 计算每个样本之间的角度增量，用于生成正弦波
    double angleIncrement = 2.0 * M_PI * FREQUENCY / SAMPLE_RATE;
    double currentAngle = 0.0; // 当前角度

    // 生成每个样本
    for (uint32_t i = 0; i < numSamples; ++i) {
        double sampleValue = AMPLITUDE * sin(currentAngle); // 计算正弦波样本值
        samples[i] = static_cast<int16_t>(sampleValue); // 转换为16位整数并存储
        currentAngle += angleIncrement; // 更新角度
        // 保持角度在0到2*PI之间，以避免浮点数精度问题累积
        if (currentAngle > 2.0 * M_PI) {
            currentAngle -= 2.0 * M_PI;
        }
    }
    return samples; // 返回生成的样本向量
}

/**
 * @brief 生成指定持续时间的静音音频样本。
 *
 * @param duration_ms 静音的持续时间 (毫秒)。
 * @return vector<int16_t> 包含静音样本 (全为0) 的向量。
 * @details 静音样本的值均为0。样本数量根据持续时间和全局 SAMPLE_RATE 计算。
 */
vector<int16_t> generateSilence(double duration_ms) {
    // 计算总样本数
    uint32_t numSamples = static_cast<uint32_t>(SAMPLE_RATE * duration_ms / 1000.0);
    // 返回一个包含numSamples个0的向量
    return vector<int16_t>(numSamples, 0);
}

// --- 新的重构函数 (New Refactored Functions) ---

/**
 * @brief 初始化应用程序参数，包括处理命令行参数和设置文件路径。
 *
 * @param argc 命令行参数数量 (来自main函数)。
 * @param argv 命令行参数数组 (来自main函数)。
 * @param args (输出参数) 用于存储解析后的文件路径的 AppArguments 结构体引用。
 * @return bool 如果成功初始化则返回true，否则返回false (例如参数数量不足)。
 * @details 此函数检查命令行参数数量是否正确。
 * 如果正确，它会从argv[1]获取输入文件路径，
 * 设置默认的配置文件名为 "audio_generator_config.json"，
 * 并根据输入文件名派生输出文件名 (例如 input.txt -> input_audio.wav)。
 */
bool initializeApplication(int argc, char* argv[], AppArguments& args) {
    if (argc < 2) { // 检查参数数量是否少于2 (程序名 + 输入文件名)
        cerr << "用法: " << argv[0] << " <input_txt_file_path>" << endl;
        cerr << "程序会自动在当前目录查找 'audio_generator_config.json' 文件以覆盖默认设置。" << endl;
        return false; // 参数不足，返回false
    }

    args.inputFilePath = argv[1]; // 设置输入文件路径
    args.configFilePath = "audio_generator_config.json"; // 设置默认配置文件路径

    // 根据输入文件路径派生输出文件路径
    size_t lastSlash = args.inputFilePath.find_last_of("/\\"); // 查找最后一个路径分隔符
    string inputFileNameBase = (lastSlash == string::npos) ? args.inputFilePath : args.inputFilePath.substr(lastSlash + 1); // 获取不带路径的文件名
    size_t lastDot = inputFileNameBase.find_last_of('.'); // 查找最后一个点 (扩展名前)
    if (lastDot != string::npos) { // 如果找到点
        inputFileNameBase = inputFileNameBase.substr(0, lastDot); // 去掉扩展名
    }
    args.outputFilePath = inputFileNameBase + "_audio.wav"; // 构造输出文件名

    return true; // 初始化成功
}

/**
 * @brief 处理输入的文本文件，并根据其内容生成音频样本。
 *
 * @param inputFilePath 要处理的输入文本文件的路径 (string)。
 * @param allSamples (输出参数) 用于存储所有生成音频样本的向量的引用。
 * @return bool 如果成功处理文件则返回true，如果无法打开输入文件则返回false。
 * @details 此函数逐字符读取输入文件：
 * - '0' 生成短哔哔声，后跟比特间静音。
 * - '1' 生成长哔哔声，后跟比特间静音。
 * - ' ' (空格) 在非首比特时，替换掉前一个比特间静音，并生成字节间静音。
 * - '\n', '\r' 被忽略。
 * - 其他字符会产生警告并被忽略。
 * 生成的样本会追加到 allSamples 向量中。
 */
bool processInputFile(const string& inputFilePath, vector<int16_t>& allSamples) {
    ifstream inputFile(inputFilePath); // 尝试打开输入文件
    if (!inputFile.is_open()) { // 如果文件打开失败
        cerr << "错误: 无法打开输入文件 '" << inputFilePath << "'" << endl;
        return false; // 返回false
    }

    cout << "正在从 '" << inputFilePath << "' 处理二进制数据并生成音频样本..." << endl;
    allSamples.clear(); // 清空样本向量，确保从头开始
    char character; // 用于存储读取的字符
    bool firstBit = true; // 标记是否为字节的第一个比特，用于处理空格前的静音逻辑

    // 逐字符读取文件
    while (inputFile.get(character)) {
        vector<int16_t> currentSamples; // 当前字符生成的音频样本
        vector<int16_t> silenceSamples; // 当前字符后的静音样本

        if (character == '0') { // 如果是 '0'
            currentSamples = generateBeep(SHORT_BEEP_DURATION_MS); // 生成短哔声
            if (BIT_SILENCE_DURATION_MS > 0) silenceSamples = generateSilence(BIT_SILENCE_DURATION_MS); // 生成比特间静音
            firstBit = false; // 不是字节首比特了
        } else if (character == '1') { // 如果是 '1'
            currentSamples = generateBeep(LONG_BEEP_DURATION_MS); // 生成长哔声
            if (BIT_SILENCE_DURATION_MS > 0) silenceSamples = generateSilence(BIT_SILENCE_DURATION_MS); // 生成比特间静音
            firstBit = false; // 不是字节首比特了
        } else if (character == ' ' && !firstBit) { // 如果是空格，并且前面已经有比特
            // 如果之前有比特间静音，且配置了比特间静音，则尝试移除它
            if (!allSamples.empty() && BIT_SILENCE_DURATION_MS > 0) {
                // 计算比特间静音对应的样本数
                uint32_t samples_to_remove = static_cast<uint32_t>(SAMPLE_RATE * BIT_SILENCE_DURATION_MS / 1000.0);
                if (allSamples.size() >= samples_to_remove) { // 确保有足够样本可移除
                    bool was_bit_silence = true; // 假设末尾是比特静音
                    // 检查末尾的样本是否确实是静音
                    for (size_t i = 0; i < samples_to_remove; ++i) {
                        if (allSamples[allSamples.size() - 1 - i] != 0) {
                            was_bit_silence = false; // 如果有非零样本，则不是纯粹的比特静音
                            break;
                        }
                    }
                    if (was_bit_silence) { // 如果确认是比特静音
                        allSamples.resize(allSamples.size() - samples_to_remove); // 移除这些静音样本
                    }
                }
            }
            // 生成字节间静音
            if (BYTE_SILENCE_DURATION_MS > 0) currentSamples = generateSilence(BYTE_SILENCE_DURATION_MS);
            silenceSamples.clear(); // 字节间静音后不应再有比特间静音
            firstBit = true; // 下一个比特将是新字节的首比特
        } else if (character == '\n' || character == '\r') { // 如果是换行符或回车符
            // 忽略
            continue;
        } else { // 其他未知字符
            cerr << "警告: 在输入文件中遇到意外字符 '" << character << "' (ASCII: " << static_cast<int>(character) << ")。已忽略。" << endl;
            continue;
        }

        // 将生成的音频和静音样本追加到总样本向量中
        if (!currentSamples.empty()) {
            allSamples.insert(allSamples.end(), currentSamples.begin(), currentSamples.end());
        }
        if (!silenceSamples.empty()) {
            allSamples.insert(allSamples.end(), silenceSamples.begin(), silenceSamples.end());
        }
    }
    inputFile.close(); // 关闭输入文件
    return true; // 处理成功
}

/**
 * @brief 将生成的音频样本写入WAV文件。
 *
 * @param outputFilePath 要写入的WAV文件的路径 (string)。
 * @param allSamples 包含所有音频样本的向量 (const vector<int16_t>&)。
 * @return bool 如果成功写入文件则返回true，否则返回false。
 * @details 如果 allSamples 为空，则不生成文件但仍返回true (视为无内容可写，非错误)。
 * 否则，它会打开输出文件，调用 writeWavHeader 写入头部，然后写入样本数据。
 */
bool writeWavOutputFile(const string& outputFilePath, const vector<int16_t>& allSamples) {
    if (allSamples.empty()) { // 如果没有样本数据
        cout << "输入未产生任何音频样本。未生成音频文件。" << endl;
        return true; // 这不是一个错误，只是没有内容可写
    }

    cout << "正在写入WAV文件: " << outputFilePath << endl;
    ofstream outputFile(outputFilePath, ios::binary); // 以二进制模式打开输出文件
    if (!outputFile.is_open()) { // 如果文件打开失败
        cerr << "错误: 无法创建或打开输出文件 '" << outputFilePath << "'" << endl;
        return false; // 返回false
    }

    // 写入WAV文件头
    writeWavHeader(outputFile, static_cast<uint32_t>(allSamples.size()));
    // 写入音频数据
    // allSamples.data() 返回指向向量内部数组的指针
    // allSamples.size() * sizeof(int16_t) 计算总字节数
    outputFile.write(reinterpret_cast<const char*>(allSamples.data()), allSamples.size() * sizeof(int16_t));
    outputFile.close(); // 关闭输出文件

    if (outputFile.good()) { // 检查文件流在关闭后的状态
        cout << "WAV文件 '" << outputFilePath << "' 生成成功!" << endl;
        return true; // 写入成功
    } else {
        cerr << "错误: 写入WAV文件 '" << outputFilePath << "' 时发生错误。" << endl;
        return false; // 写入失败
    }
}

/**
 * @brief 程序主入口点。
 *
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return int 程序退出状态码 (0表示成功, 非0表示错误)。
 * @details
 * 1. 初始化应用程序参数 (initializeApplication)。
 * 2. 加载配置文件 (loadConfiguration)。
 * 3. 处理输入文件并生成音频样本 (processInputFile)，同时计时。
 * 4. 将生成的音频样本写入WAV文件 (writeWavOutputFile)。
 */
int main(int argc, char* argv[]) {
    AppArguments appArgs; // 创建AppArguments结构体实例

    // 1. 初始化应用程序参数和文件路径
    if (!initializeApplication(argc, argv, appArgs)) {
        return 1; // 如果初始化失败，退出程序 (错误信息已由函数打印)
    }

    // 打印确认的文件路径信息
    cout << "输入文件: " << appArgs.inputFilePath << endl;
    cout << "输出文件将是: " << appArgs.outputFilePath << endl;
    cout << "配置文件: " << appArgs.configFilePath << endl;

    // 2. 加载配置 (会更新全局配置变量)
    loadConfiguration(appArgs.configFilePath);

    vector<int16_t> allSamples; // 用于存储所有音频样本的向量
    auto startTime = chrono::high_resolution_clock::now(); // 获取处理开始时间

    // 3. 处理输入文件并生成音频样本
    if (!processInputFile(appArgs.inputFilePath, allSamples)) {
        // 如果处理失败，退出程序 (错误信息已由函数打印)
        return 1;
    }

    auto endTime = chrono::high_resolution_clock::now(); // 获取处理结束时间
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime); // 计算处理耗时
    cout << "处理时间: " << duration.count() << " 毫秒" << endl;

    // 4. 将音频样本写入WAV文件
    if (!writeWavOutputFile(appArgs.outputFilePath, allSamples)) {
        // 如果写入失败，退出程序 (错误信息已由函数打印)
        return 1;
    }

    return 0; // 程序成功结束
}
