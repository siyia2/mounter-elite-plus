#include "sanitization_readline.h"
#include "conversion_tools.h"


std::vector<std::string> binImgFilesCache; // Memory cached binImgFiles here
std::vector<std::string> mdfMdsFilesCache; // Memory cached mdfImgFiles here


// BIN/IMG CONVERSION FUNCTIONS	\\

// Function to list available files and prompt the user to choose a file for conversion
std::string chooseFileToConvert(const std::vector<std::string>& files) {
    // Display a header indicating the available .bin and .img files
    std::cout << "\033[32mFound the following .bin and .img files:\033[0m\n";

    // Iterate through the files and display them with their corresponding numbers
    for (size_t i = 0; i < files.size(); ++i) {
        std::cout << i + 1 << ": " << files[i] << "\n";
    }

    // Prompt the user to enter the number of the file they want to convert
    int choice;
    std::cout << "\033[94mEnter the number of the file you want to convert:\033[0m ";
    std::cin >> choice;

    // Check if the user's choice is within the valid range
    if (choice >= 1 && choice <= static_cast<int>(files.size())) {
        // Return the chosen file path
        return files[choice - 1];
    } else {
        // Print an error message for an invalid choice
        std::cout << "\033[31mInvalid choice. Please choose a valid file.\033[31m\n";
        return "";
    }
}


std::vector<std::string> findBinImgFiles(const std::vector<std::string>& directories, const std::vector<std::string>& previousPaths) {
    // Check if the cache is already populated and return it if available
    if (!previousPaths.empty() && directories == previousPaths && !binImgFilesCache.empty()) {
        return binImgFilesCache;
    }

    // Vector to store the file names found
    std::vector<std::string> fileNames;

    try {
        std::vector<std::future<void>> futures; // Vector to hold asynchronous tasks
        std::mutex mutex; // Mutex for thread-safe access to shared data
        const int maxThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2; // Maximum threads supported
        const int batchSize = 2; // Tweak the batch size as needed

        // Iterate through the specified directories
        for (const auto& directory : directories) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    // Get the file extension and convert it to lowercase
                    std::string ext = entry.path().extension();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
                        return std::tolower(c);
                    });

                    // Check conditions for a valid .bin or .img file
                    if ((ext == ".bin" || ext == ".img") &&
                        (entry.path().filename().string().find("data") == std::string::npos) &&
                        (entry.path().filename().string() != "terrain.bin") &&
                        (entry.path().filename().string() != "blocklist.bin")) {

                        // Check file size before adding to the list
                        if (std::filesystem::file_size(entry) >= 10'000'000) {
                            // Ensure the number of active threads doesn't exceed the maximum
                            while (futures.size() >= maxThreads) {
                                auto it = std::find_if(futures.begin(), futures.end(),
                                    [](const std::future<void>& f) {
                                        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                                    });
                                if (it != futures.end()) {
                                    it->get();
                                    futures.erase(it);
                                }
                            }

                            // Asynchronously add the file name to the list
                            futures.push_back(std::async(std::launch::async, [entry, &fileNames, &mutex] {
                                std::string fileName = entry.path().string();

                                std::lock_guard<std::mutex> lock(mutex);
                                fileNames.push_back(fileName);
                            }));

                            // Check if the batch size is reached, and wait for completed tasks
                            if (futures.size() >= batchSize) {
                                for (auto& future : futures) {
                                    future.get();
                                }
                                futures.clear();
                            }
                        }
                    }
                }
            }
        }

        // Wait for any remaining asynchronous tasks to complete
        for (auto& future : futures) {
            future.get();
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    // Cache the file names for future use
    binImgFilesCache = fileNames;
    return fileNames;
}


// Check if ccd2iso is installed on the system
bool isCcd2IsoInstalled() {
    // Use the system command to check if ccd2iso is available
    if (std::system("which ccd2iso > /dev/null 2>&1") == 0) {
        return true; // ccd2iso is installed
    } else {
        return false; // ccd2iso is not installed
    }
}


// Function to convert a BIN file to ISO format
void convertBINToISO(const std::string& inputPath) {
    // Check if the input file exists
    if (!std::ifstream(inputPath)) {
        std::cout << "\033[31mThe specified input file '" << inputPath << "' does not exist.\033[0m" << std::endl;
        return;
    }

    // Define the output path for the ISO file with only the .iso extension
    std::string outputPath = inputPath.substr(0, inputPath.find_last_of(".")) + ".iso";

    // Check if the output ISO file already exists
    if (std::ifstream(outputPath)) {
        std::cout << "\033[33mThe output ISO file '" << outputPath << "' already exists. Skipping conversion.\033[0m" << std::endl;
        return;  // Skip conversion if the file already exists
    }

    // Execute the conversion using ccd2iso, with shell-escaped paths
    std::string conversionCommand = "ccd2iso " + shell_escape(inputPath) + " " + shell_escape(outputPath);
    int conversionStatus = std::system(conversionCommand.c_str());

    // Check the result of the conversion
    if (conversionStatus == 0) {
        std::cout << "\033[32mImage file converted to ISO:\033[0m " << outputPath << std::endl;
    } else {
        std::cout << "\033[31mConversion of " << inputPath << " failed.\033[0m" << std::endl;

        // Delete the partially created ISO file
        if (std::remove(outputPath.c_str()) == 0) {
            std::cout << "\033[31mDeleted partially created ISO file:\033[0m " << outputPath << std::endl;
        } else {
            std::cerr << "\033[31mFailed to delete partially created ISO file:\033[0m " << outputPath << std::endl;
        }
    }
}


// Function to convert multiple BIN files to ISO format concurrently
void convertBINsToISOs(const std::vector<std::string>& inputPaths, int numThreads) {
    // Check if ccd2iso is installed on the system
    if (!isCcd2IsoInstalled()) {
        std::cout << "\033[31mccd2iso is not installed. Please install it before using this option.\033[0m" << std::endl;
        return;
    }

    // Create a thread pool with a limited number of threads
    std::vector<std::thread> threads;
    int numCores = std::min(numThreads, static_cast<int>(std::thread::hardware_concurrency()));

    for (const std::string& inputPath : inputPaths) {
        if (inputPath == "") {
            break; // Break the loop if an empty path is encountered
        } else {
            // Construct the shell-escaped input path
            std::string escapedInputPath = shell_escape(inputPath);

            // Create a new thread for each conversion
            threads.emplace_back(convertBINToISO, escapedInputPath);

            if (threads.size() >= numCores) {
                // Limit the number of concurrent threads to the number of available cores
                for (auto& thread : threads) {
                    thread.join();
                }
                threads.clear();
            }
        }
    }

    // Join any remaining threads
    for (auto& thread : threads) {
        thread.join();
    }
}


// Function to process a range of files and convert them to ISO format
void processFilesInRange(int start, int end) {
    // Get a list of BIN/IMG files
	std::vector<std::string> binImgFiles;
    
    // Determine the number of threads based on CPU cores
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2;

    // Select files within the specified range
    std::vector<std::string> selectedFiles;
    for (int i = start; i <= end; i++) {
        selectedFiles.push_back(binImgFiles[i - 1]);
    }

    // Construct the shell-escaped file paths
    std::vector<std::string> escapedSelectedFiles;
    for (const std::string& file : selectedFiles) {
        escapedSelectedFiles.push_back(shell_escape(file));
    }

    // Call convertBINsToISOs with shell-escaped file paths
    convertBINsToISOs(escapedSelectedFiles, numThreads);
}


// Main function to select directories and convert BIN/IMG files to ISO format
void select_and_convert_files_to_iso() {
    // Initialize vectors to store BIN/IMG files and directory paths
    std::vector<std::string> binImgFiles;
    std::vector<std::string> directoryPaths;
    
    // Declare previousPaths as a static variable
    static std::vector<std::string> previousPaths;
    
    // Read input for directory paths (allow multiple paths separated by spaces)
    std::string inputPaths = readInputLine("\033[94mEnter the directory paths (separated by spaces) to search for .bin .img files or simply press enter to return:\033[0m ");
    std::istringstream iss(inputPaths);

    // Populate the directoryPaths vector
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),
              std::back_inserter(directoryPaths));

    // Check if directoryPaths is empty
    if (directoryPaths.empty()) {
        std::cout << "Path input is empty. Exiting." << std::endl;
        return;
    }

    // Call the findBinImgFiles function to populate the cache
    binImgFiles = findBinImgFiles(directoryPaths, previousPaths);

    // Check if binImgFiles is empty
    if (binImgFiles.empty()) {
        std::cout << "\033[33mNo .bin or .img files found in the specified directories and their subdirectories or all files are under 10MB.\n\033[0m";
    } else {
        // String for user input
        std::string input;

        while (true) {
            std::system("clear");
            // Print the list of BIN/IMG files
            printFileListBin(binImgFiles);
            // Prompt user to choose a file or exit
            std::cout << "\033[94mChoose a file to process (enter the number or range e.g., 1-5 or 1 or simply press Enter to return):\033[0m ";
            std::getline(std::cin, input);

            // Break the loop if the user presses Enter
            if (input.empty()) {
                std::system("clear");
                break;
            }
			std::system("clear");
            // Process user input
            processInputBin(input, binImgFiles);
            std::cout << "Press enter to continue...";
            std::cin.ignore();
        }
    }
}


// Function to print the list of BIN/IMG files
void printFileListBin(const std::vector<std::string>& fileList) {
    for (int i = 0; i < fileList.size(); i++) {
        std::cout << i + 1 << ". " << fileList[i] << std::endl;
    }
}


// Function to process user input and convert selected BIN/IMG files to ISO format
void processInputBin(const std::string& input, const std::vector<std::string>& fileList) {
    // Tokenize the input string
    std::istringstream iss(input);
    std::string token;
    std::vector<std::thread> threads;

    // Iterate over tokens
    while (iss >> token) {
        std::istringstream tokenStream(token);
        int start, end;
        char dash;

        // Attempt to parse the token as a number
        if (tokenStream >> start) {
            // Check for a range input (e.g., 1-5)
            if (tokenStream >> dash && dash == '-' && tokenStream >> end) {
                // Process a valid range input
                if (start >= 1 && start <= fileList.size() && end >= start && end <= fileList.size()) {
                    for (int i = start; i <= end; i++) {
                        int selectedIndex = i - 1;
                        std::string selectedFile = fileList[selectedIndex];
                        threads.emplace_back(convertBINToISO, selectedFile);
                    }
                } else {
                    std::cout << "\033[31mInvalid range. Please try again.\033[0m" << std::endl;
                    
                }
            } else if (start >= 1 && start <= fileList.size()) {
                // Process a valid single number input
                int selectedIndex = start - 1;
                std::string selectedFile = fileList[selectedIndex];
                threads.emplace_back(convertBINToISO, selectedFile);
            } else {
                // Handle invalid number input
                std::cout << "\033[31mInvalid number: " << start << ". Please try again.\033[0m" << std::endl;
            }
        } else {
            // Handle invalid input format
            std::cout << "\033[31mInvalid input format: Please try again.\033[0m" << std::endl;
        }
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
}


// MDF/MDS CONVERSION FUNCTIONS	\\

std::vector<std::string> findMdsMdfFiles(const std::string& paths) {
    // Parse input paths
    std::vector<std::string> directories;
    std::istringstream iss(paths);
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),
              std::back_inserter(directories));

    // Check if no paths are provided
    if (directories.empty()) {
        std::cerr << "No paths provided. Exiting." << std::endl;
        return {};  // Return an empty vector or handle it as needed
    }

    // Static variable to store cached paths
    static std::vector<std::string> cachedPaths;

    // Check if the cache is already populated and if the input paths are the same
    if (!mdfMdsFilesCache.empty() && directories == cachedPaths) {
        return mdfMdsFilesCache;
    }

    std::vector<std::string> fileNames;
    try {
        std::vector<std::future<void>> futures;
        std::mutex mutex;
        const int maxThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2;
        const int batchSize = 2; // Tweak the batch size as needed

        // Iterate over specified directories
        for (const auto& directory : directories) {
            // Iterate over files in the directory and its subdirectories
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                // Check if the entry is a regular file
                if (entry.is_regular_file()) {
                    // Check file extension and size
                    std::string ext = entry.path().extension();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
                        return std::tolower(c);
                    });

                    if (ext == ".mdf") {
                        // Process the file if it meets size criteria
                        if (std::filesystem::file_size(entry) >= 10'000'000) {
                            // Limit the number of concurrent tasks
                            while (futures.size() >= maxThreads) {
                                auto it = std::find_if(futures.begin(), futures.end(),
                                    [](const std::future<void>& f) {
                                        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                                    });
                                if (it != futures.end()) {
                                    it->get();
                                    futures.erase(it);
                                }
                            }

                            // Create a task to process the file
                            futures.push_back(std::async(std::launch::async, [entry, &fileNames, &mutex] {
                                std::string fileName = entry.path().string();

                                std::lock_guard<std::mutex> lock(mutex);
                                fileNames.push_back(fileName);
                            }));

                            // Check the batch size and acquire the lock to merge
                            if (futures.size() >= batchSize) {
                                for (auto& future : futures) {
                                    future.get();
                                }
                                futures.clear();
                            }
                        }
                    }
                }
            }
        }

        // Wait for any remaining tasks
        for (auto& future : futures) {
            future.get();
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    // Update the cache only if the input paths are different
    if (directories != cachedPaths) {
        mdfMdsFilesCache = fileNames;
        cachedPaths = directories;  // Update the cached paths
    }

    return fileNames;
}


// Function to check if mdf2iso is installed
bool isMdf2IsoInstalled() {
    // Construct a command to check if mdf2iso is in the system's PATH
    std::string command = "which " + shell_escape("mdf2iso");

    // Execute the command and check the result
    if (std::system((command + " > /dev/null 2>&1").c_str()) == 0) {
        return true;  // mdf2iso is installed
    } else {
        return false;  // mdf2iso is not installed
    }
}


// Function to convert an MDF file to ISO format using mdf2iso
void convertMDFToISO(const std::string& inputPath) {
    // Check if the input file exists
    if (!std::ifstream(inputPath)) {
        std::cout << "\033[31mThe specified input file '" << inputPath << "' does not exist.\033[0m" << std::endl;
        return;
    }

    // Check if the corresponding .iso file already exists
    std::string isoOutputPath = inputPath.substr(0, inputPath.find_last_of(".")) + ".iso";
    if (std::ifstream(isoOutputPath)) {
        std::cout << "\033[33mThe corresponding .iso file already exists for '" << inputPath << "'. Skipping conversion.\033[0m" << std::endl;
        return;
    }

    // Escape the inputPath before using it in shell commands
    std::string escapedInputPath = shell_escape(inputPath);

    // Define the output path for the ISO file with only the .iso extension
    std::string outputPath = inputPath.substr(0, inputPath.find_last_of(".")) + ".iso";

    // Escape the outputPath before using it in shell commands
    std::string escapedOutputPath = shell_escape(outputPath);

    // Continue with the rest of the conversion logic...

    // Execute the conversion using mdf2iso
    std::string conversionCommand = "mdf2iso " + escapedInputPath + " " + escapedOutputPath;
	
    // Capture the output of the mdf2iso command
    FILE* pipe = popen(conversionCommand.c_str(), "r");
    if (!pipe) {
        std::cout << "\033[31mFailed to execute conversion command\033[0m" << std::endl;
        return;
    }

    char buffer[128];
    std::string conversionOutput;
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        conversionOutput += buffer;
    }

    int conversionStatus = pclose(pipe);

    if (conversionStatus == 0) {
        // Check if the conversion output contains the "already ISO9660" message
        if (conversionOutput.find("already ISO") != std::string::npos) {
            std::cout << "\033[31mThe selected file '" << inputPath << "' is already in ISO format, maybe rename it to .iso?. Skipping conversion.\033[0m" << std::endl;
        } else {
            std::cout << "\033[32mImage file converted to ISO:\033[0m " << outputPath << std::endl;
        }
    } else {
        std::cout << "\033[31mConversion of " << inputPath << " failed.\033[0m" << std::endl;
    }
}

// Function to convert multiple MDF files to ISO format using mdf2iso
void convertMDFsToISOs(const std::vector<std::string>& inputPaths, int numThreads) {
    // Check if mdf2iso is installed
    if (!isMdf2IsoInstalled()) {
        std::cout << "\033[31mmdf2iso is not installed. Please install it before using this option.\033[0m";
        return;
    }

    // Create a thread pool with a limited number of threads
    std::vector<std::thread> threads;
    int numCores = std::min(numThreads, static_cast<int>(std::thread::hardware_concurrency()));

    for (const std::string& inputPath : inputPaths) {
        if (inputPath == "") {
            break; // Exit the loop
        } else {
            // No need to escape the file path, as we'll handle it in the convertMDFToISO function
            std::string escapedInputPath = inputPath;

            // Create a new thread for each conversion
            threads.emplace_back(convertMDFToISO, escapedInputPath);

            // Limit the number of concurrent threads to the number of available cores
            if (threads.size() >= numCores) {
                for (auto& thread : threads) {
                    thread.join();
                }
                threads.clear();
            }
        }
    }

    // Join any remaining threads
    for (auto& thread : threads) {
        thread.join();
    }
}

// Function to process a range of MDF files by converting them to ISO
void processMDFFilesInRange(int start, int end) {
    std::vector<std::string> mdfImgFiles;	// Declare mdfImgFiles here
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2; // Determine the number of threads based on CPU cores
    std::vector<std::string> selectedFiles;

    // Construct a list of selected files based on the specified range
    for (int i = start; i <= end; i++) {
        std::string filePath = (mdfImgFiles[i - 1]);
        selectedFiles.push_back(filePath);
    }

    // Call the function to convert selected MDF files to ISO
    convertMDFsToISOs(selectedFiles, numThreads);
}

// Function to interactively select and convert MDF files to ISO
void select_and_convert_files_to_iso_mdf() {
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2;
    std::string directoryPath = readInputLine("\033[94mEnter the directory paths (separated by spaces) to search for .mdf files or simply press enter to return:\033[0m ");

    if (directoryPath.empty()) {
        return;
    }

    // Call the findMdsMdfFiles function to populate the cache
    std::vector<std::string> mdfMdsFiles = findMdsMdfFiles(directoryPath);

    if (mdfMdsFiles.empty()) {
        std::cout << "\033[33mNo .mdf files found in the specified directory and its subdirectories or all files are under 10MB.\n\033[0m";
        return;
    }

    // Continue selecting and converting files until the user decides to exit
    while (true) {
		std::system("clear");
        printFileListMdf(mdfMdsFiles);

        // Prompt the user to enter file numbers or 'exit'
        std::string input = readInputLine("\033[94mEnter the numbers of the files to convert (e.g., '1-2' or '1 2', or simply press enter to return):\033[0m ");

        if (input.empty()) {
			std::system("clear");
            break;
        }

        // Parse the user input to get selected file indices
        std::vector<int> selectedFileIndices = parseUserInput(input, mdfMdsFiles.size());

        if (!selectedFileIndices.empty()) {
            // Get the paths of the selected files based on user input
            std::vector<std::string> selectedFiles = getSelectedFiles(selectedFileIndices, mdfMdsFiles);
			std::system("clear");
            // Convert the selected MDF files to ISO
            convertMDFsToISOs(selectedFiles, numThreads);
            std::cout << "Press enter to continue...";
		std::cin.ignore();
        } else {
            std::cout << "Invalid choice. Please enter valid file numbers or 'exit'." << std::endl;
        }
    }
}

// Function to print the list of MDF files with their corresponding indices
void printFileListMdf(const std::vector<std::string>& fileList) {
    std::cout << "Select files to convert to ISO:\n";
    for (int i = 0; i < fileList.size(); i++) {
        std::cout << i + 1 << ". " << fileList[i] << std::endl;
    }
}

// Function to parse user input and extract selected file indices
std::vector<int> parseUserInput(const std::string& input, int maxIndex) {
    std::vector<int> selectedFileIndices;
    std::istringstream iss(input);
    std::string token;

    // Iterate through the tokens in the input string
    while (iss >> token) {
        if (token.find('-') != std::string::npos) {
            // Handle a range (e.g., "1-2")
            size_t dashPos = token.find('-');
            int startRange = std::stoi(token.substr(0, dashPos));
            int endRange = std::stoi(token.substr(dashPos + 1));

            // Add each index within the specified range to the selected indices vector
            for (int i = startRange; i <= endRange; i++) {
                if (i >= 1 && i <= maxIndex) {
                    selectedFileIndices.push_back(i - 1);
                }
            }
        } else {
            // Handle individual numbers (e.g., "1")
            int selectedFileIndex = std::stoi(token);

            // Add the index to the selected indices vector if it is within the valid range
            if (selectedFileIndex >= 1 && selectedFileIndex <= maxIndex) {
                selectedFileIndices.push_back(selectedFileIndex - 1);
            }
        }
    }

    return selectedFileIndices;
}

// Function to retrieve selected files based on their indices
std::vector<std::string> getSelectedFiles(const std::vector<int>& selectedIndices, const std::vector<std::string>& fileList) {
    std::vector<std::string> selectedFiles;

    // Iterate through the selected indices and add corresponding files to the selected files vector
    for (int index : selectedIndices) {
        selectedFiles.push_back(fileList[index]);
    }

    return selectedFiles;
}