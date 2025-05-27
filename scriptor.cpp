#include <iostream> // 用于标准输入输出流 (cin, cout, cerr)
#include <fstream>  // 用于文件流 (ifstream, ofstream)
#include <string>   // 用于字符串操作

/**
 * @brief 将单个无符号字符转换为其8位二进制字符串表示形式。
 *
 * @param c 要转换的无符号字符 (unsigned char)。
 * @return std::string 表示字符的8位二进制数的字符串。
 * 例如，字符 'A' (ASCII 65) 会被转换为 "01000001"。
 */
std::string charToBinaryString(unsigned char c) {
    std::string binaryString = ""; // 初始化空字符串用于存储二进制结果
    // 从最高位 (第7位) 到最低位 (第0位) 遍历
    for (int i = 7; i >= 0; --i) {
        // 检查当前位是否为1
        // (c & (1 << i)) 通过位与操作检查第i位
        // (1 << i) 创建一个掩码，其中只有第i位是1
        if ((c & (1 << i)) != 0) {
            binaryString += '1'; // 如果第i位是1，追加 '1'
        } else {
            binaryString += '0'; // 否则，追加 '0'
        }
    }
    return binaryString; // 返回转换后的二进制字符串
}

/**
 * @brief 程序主入口函数。
 *
 * @param argc 命令行参数的数量。
 * @param argv 命令行参数的数组 (字符串)。
 * @return int 程序退出状态码 (0 表示成功, 非0 表示错误)。
 */
int main(int argc, char* argv[]) {
    // --- 参数解析 (Argument Parsing) ---

    // 检查是否没有提供输入文件路径
    if (argc < 2) { // 如果参数数量小于2 (意味着只有程序名，没有输入文件路径)
        std::cerr << "Error: No input file path provided. Please provide a path to an input file." << std::endl; // 打印错误信息：未提供输入文件路径
        std::cerr << "Usage: " << argv[0] << " <inputFilePath> [outputFilePath]" << std::endl; // 打印基本用法
        return 1; // 返回错误码1，表示程序执行失败
    }

    // 检查是否提供了过多的参数
    if (argc > 3) { // 如果参数数量大于3 (程序名 + 输入路径 + 输出路径 + 更多参数)
        std::cerr << "Error: Too many arguments." << std::endl; // 打印错误信息：参数过多
        std::cerr << "Usage: " << argv[0] << " <inputFilePath> [outputFilePath]" << std::endl; // 打印详细用法说明
        std::cerr << "  <inputFilePath>  : Path to the input text file." << std::endl;
        std::cerr << "  [outputFilePath] : Optional. Path for the output binary file." << std::endl;
        std::cerr << "                   If not provided, output defaults to 'binary.txt' in the current directory." << std::endl;
        return 1; // 返回错误码1
    }

    std::string inputFilePath = argv[1]; // 从命令行参数获取输入文件路径
    std::string outputFilePath;          // 声明输出文件路径字符串

    if (argc == 3) { // 如果提供了三个参数 (程序名 + 输入路径 + 输出路径)
        outputFilePath = argv[2]; // 使用用户提供的输出文件路径
    } else { // 否则 (只有两个参数：程序名 + 输入路径)
        outputFilePath = "binary.txt"; // 使用默认的输出文件名 "binary.txt"
    }

    // 尝试以二进制模式打开输入文件进行读取
    // std::ios::binary 标志确保文件按原样读取，不做任何文本模式的转换 (如换行符处理)
    std::ifstream inputFile(inputFilePath, std::ios::binary);

    // 检查输入文件是否成功打开
    if (!inputFile.is_open()) { // 如果文件未能打开
        std::cerr << "Error: Unable to open input file '" << inputFilePath << "'. Please check if the path is correct and the file exists." << std::endl; // 打印错误信息
        return 1; // 返回错误码1
    }

    // 尝试创建 (或覆盖) 输出文件进行写入
    std::ofstream outputFile(outputFilePath);

    // 检查输出文件是否成功打开/创建
    if (!outputFile.is_open()) { // 如果文件未能打开/创建
        std::cerr << "Error: Unable to create or open output file '" << outputFilePath << "'." << std::endl; // 打印错误信息
        inputFile.close(); // 关闭已经打开的输入文件，避免资源泄露
        return 1; // 返回错误码1
    }

    // --- 文件处理 (File Processing) ---
    char character; // 用于存储从输入文件读取的单个字符
    // 逐个字符读取输入文件，直到文件末尾
    while (inputFile.get(character)) { // inputFile.get(char) 读取一个字符并将其存入 'character'
        // 将读取的 char 转换为 unsigned char
        // 这是因为 charToBinaryString 函数期望 unsigned char 以确保位操作的一致性 (char 可能是有符号的)
        unsigned char u_char = static_cast<unsigned char>(character);

        // 将字符的二进制字符串表示写入输出文件
        outputFile << charToBinaryString(u_char);
        // 在每个8位二进制码后添加一个空格，以便于阅读
        outputFile << " ";
    }

    // 检查读取过程中是否发生除到达文件末尾 (EOF) 之外的错误
    if (!inputFile.eof() && inputFile.fail()) { // eof() 检查是否到达文件末尾，fail() 检查流是否发生逻辑错误或格式错误
        std::cerr << "Warning: An error occurred while reading the input file." << std::endl; // 打印警告信息
        // 程序仍将尝试关闭文件并正常退出
    }

    // 关闭文件流
    inputFile.close();  // 关闭输入文件流，释放资源
    outputFile.close(); // 关闭输出文件流，确保所有缓冲数据都已写入磁盘

    // --- 添加成功消息 (Add success message) ---
    std::cout << "File conversion successful!" << std::endl; // 打印文件转换成功的消息
    std::cout << "Binary output has been saved to: " << outputFilePath << std::endl; // 告知用户输出文件的保存位置

    return 0; // 程序成功完成，返回0
}
