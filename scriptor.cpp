#include <iostream>
#include <fstream>
#include <string>

// Function: Converts a single character to its 8-bit binary string representation
std::string charToBinaryString(unsigned char c) {
    std::string binaryString = "";
    for (int i = 7; i >= 0; --i) {
        if ((c & (1 << i)) != 0) {
            binaryString += '1';
        } else {
            binaryString += '0';
        }
    }
    return binaryString;
}

int main(int argc, char* argv[]) {
    // --- Argument Parsing ---
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <inputFilePath> [outputFilePath]" << std::endl;
        std::cerr << "  <inputFilePath>  : Path to the input text file." << std::endl;
        std::cerr << "  [outputFilePath] : Optional. Path for the output binary file." << std::endl;
        std::cerr << "                     If not provided, output defaults to 'binary.txt' in the current directory." << std::endl;
        return 1; // Return error code
    }

    std::string inputFilePath = argv[1];
    std::string outputFilePath;

    if (argc == 3) {
        outputFilePath = argv[2];
    } else {
        outputFilePath = "binary.txt"; // Default output file name
    }

    // Attempt to open the input file for reading in binary mode
    std::ifstream inputFile(inputFilePath, std::ios::binary);

    // Check if the input file was opened successfully
    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open input file '" << inputFilePath << "'. Please check if the path is correct and the file exists." << std::endl;
        return 1; // Return error code
    }

    // Attempt to create (or overwrite) the output file for writing
    std::ofstream outputFile(outputFilePath);

    // Check if the output file was opened/created successfully
    if (!outputFile.is_open()) {
        std::cerr << "Error: Unable to create or open output file '" << outputFilePath << "'." << std::endl;
        inputFile.close(); // Close the already opened input file
        return 1; // Return error code
    }

    // --- File Processing ---
    char character;
    // Read the input file character by character
    while (inputFile.get(character)) {
        // Convert the read character to an unsigned char
        unsigned char u_char = static_cast<unsigned char>(character);

        // Convert the character's binary string to the output file
        outputFile << charToBinaryString(u_char);
        // Add a space after each 8-bit binary code
        outputFile << " ";
    }

    // Check for errors during reading (other than reaching end-of-file)
    if (!inputFile.eof() && inputFile.fail()) {
        std::cerr << "Warning: An error occurred while reading the input file." << std::endl;
        // Program will still attempt to close files and exit gracefully
    }

    // Close the file streams
    inputFile.close();
    outputFile.close();

    // --- Add success message ---
    std::cout << "File conversion successful!" << std::endl;
    std::cout << "Binary output has been saved to: " << outputFilePath << std::endl;

    return 0; // Program finished successfully
}