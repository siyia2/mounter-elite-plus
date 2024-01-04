#include "sanitization_readline.h"
#include "conversion_tools.h"


static std::vector<std::string> binImgFilesCache; // Memory cached binImgFiles here
static std::vector<std::string> mdfMdsFilesCache; // Memory cached mdfImgFiles here

std::vector<std::string> binImgFiles; // binImgFiles here

std::mutex mdfFilesMutex; // Mutex to protect access to mdfImgFiles
std::mutex binImgFilesMutex; // Mutex to protect access to binImgFiles


// BIN/IMG CONVERSION FUNCTIONS	\\

// Function to list available files and prompt the user to choose a file for conversion
std::string chooseFileToConvert(const std::vector<std::string>& files) {
    // Display a header indicating the available .bin and .img files
    std::cout << "\033[92mFound the following .bin and .img files:\033[0m\n";

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
        std::cout << "\033[91mInvalid choice. Please choose a valid file.\033[91m\n";
        return "";
    }
}

// Function to search for .bin and .img files under 10MB
std::vector<std::string> findBinImgFiles(std::vector<std::string>& paths, const std::function<void(const std::string&, const std::string&)>& callback) {
    // Vector to store cached invalid paths
    static std::vector<std::string> cachedInvalidPaths;

    // Static variables to cache results for reuse
    static std::vector<std::string> binImgFilesCache;

    // Vector to store file names that match the criteria
    std::vector<std::string> fileNames;

    // Clear the cachedInvalidPaths before processing a new set of paths
    cachedInvalidPaths.clear();

    bool printedEmptyLine = false;  // Flag to track if an empty line has been printed

    try {
        // Mutex to ensure thread safety
        static std::mutex mutex;

        // Determine the maximum number of threads to use based on hardware concurrency; fallback is 2 threads
        const int maxThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2;

        // Use a thread pool for parallel processing
        std::vector<std::future<void>> futures;

        // Iterate through input paths
        for (const auto& path : paths) {
            try {
                // Use a lambda function to process files asynchronously
                auto processFileAsync = [&](const std::filesystem::directory_entry& entry) {
                    std::string fileName = entry.path().string();
                    std::string filePath = entry.path().parent_path().string();  // Get the path of the directory

                    // Call the callback function to inform about the found file
                    callback(fileName, filePath);

                    // Lock the mutex to ensure safe access to shared data (fileNames)
                    std::lock_guard<std::mutex> lock(mutex);
                    fileNames.push_back(fileName);
                };

                // Use async to process files concurrently
                futures.clear();

                // Iterate through files in the given directory and its subdirectories
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        // Check if the file has a ".bin" or ".img" extension and is larger than or equal to 10,000,000 bytes
                        std::string ext = entry.path().extension();
                        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
                            return std::tolower(c);
                        });

                        if ((ext == ".bin" || ext == ".img") && std::filesystem::file_size(entry) >= 10'000'000) {
                            // Check if the file is already present in the cache to avoid duplicates
                            std::string fileName = entry.path().string();
                            if (std::find(binImgFilesCache.begin(), binImgFilesCache.end(), fileName) == binImgFilesCache.end()) {
                                // Process the file asynchronously
                                futures.emplace_back(std::async(std::launch::async, processFileAsync, entry));
                            }
                        }
                    }
                }

                // Wait for all asynchronous tasks to complete
                for (auto& future : futures) {
                    future.get();
                }

            } catch (const std::filesystem::filesystem_error& e) {
                // Handle filesystem errors for the current directory
                if (!printedEmptyLine) {
                    // Print an empty line before starting to print invalid paths (only once)
                    std::cout << " " << std::endl;
                    printedEmptyLine = true;
                }
                if (std::find(cachedInvalidPaths.begin(), cachedInvalidPaths.end(), path) == cachedInvalidPaths.end()) {
                    std::cerr << "\033[91mInvalid directory path: '" << path << "'. Excluded from search." << "\033[0m" << std::endl;
                    // Add the invalid path to cachedInvalidPaths to avoid duplicate error messages
                    cachedInvalidPaths.push_back(path);
                }
            }
        }

    } catch (const std::filesystem::filesystem_error& e) {
        if (!printedEmptyLine) {
            // Print an empty line before starting to print invalid paths (only once)
            std::cout << " " << std::endl;
            printedEmptyLine = true;
        }
        // Handle filesystem errors for the overall operation
        std::cerr << "\033[91m" << e.what() << "\033[0m" << std::endl;
        std::cin.ignore();
    }

    // Print success message if files were found
    if (!fileNames.empty()) {
        std::cout << " " << std::endl;
        std::cout << "\033[92mFound " << fileNames.size() << " matching file(s)\033[0m" << ".\033[93m " << binImgFilesCache.size() << " matching file(s) cached in RAM from previous searches.\033[0m" << std::endl;
        std::cout << " " << std::endl;
        std::cout << "Press enter to continue...";
        std::cin.ignore();
    }

    // Remove duplicates from fileNames by sorting and using unique erase idiom
    std::sort(fileNames.begin(), fileNames.end());
    fileNames.erase(std::unique(fileNames.begin(), fileNames.end()), fileNames.end());

    // Update the cache by appending fileNames to binImgFilesCache
    binImgFilesCache.insert(binImgFilesCache.end(), fileNames.begin(), fileNames.end());

    // Return the combined results
    return binImgFilesCache;
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
        std::cout << "\033[91mThe specified input file \033[93m'" << inputPath << "'\033[91m does not exist.\033[0m" << std::endl;
        return;
    }

    // Define the output path for the ISO file with only the .iso extension
    std::string outputPath = inputPath.substr(0, inputPath.find_last_of(".")) + ".iso";

    // Check if the output ISO file already exists
    if (std::ifstream(outputPath)) {
        std::cout << "\033[93mThe corresponding .iso file already exists for: \033[92m'" << outputPath << "'\033[93m. Skipping conversion.\033[0m" << std::endl;
        return;  // Skip conversion if the file already exists
    }

    // Execute the conversion using ccd2iso, with shell-escaped paths
    std::string conversionCommand = "ccd2iso " + shell_escape(inputPath) + " " + shell_escape(outputPath);
    int conversionStatus = std::system(conversionCommand.c_str());

    // Check the result of the conversion
    if (conversionStatus == 0) {
        std::cout << "Image file converted to ISO:\033[0m \033[92m'" << outputPath << "'\033[0m.\033[0m" << std::endl;
    } else {
        std::cout << "\033[91mConversion of \033[93m'" << inputPath << "'\033[91m failed.\033[0m" << std::endl;

        // Delete the partially created ISO file
        if (std::remove(outputPath.c_str()) == 0) {
            std::cout << "\033[91mDeleted partially created ISO file:\033[93m '" << outputPath << "'\033[91m failed.\033[0m" << std::endl;
        } else {
            std::cerr << "\033[91mFailed to delete partially created ISO file: '" << outputPath << "'.\033[0m" << std::endl;
        }
    }
}


// Function to convert multiple BIN files to ISO format concurrently
void convertBINsToISOs(const std::vector<std::string>& inputPaths, int numThreads) {
    // Check if ccd2iso is installed on the system
    if (!isCcd2IsoInstalled()) {
        std::cout << "\033[91mccd2iso is not installed. Please install it before using this option.\033[0m" << std::endl;
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
    // Determine the number of threads based on CPU cores fallback is 2 threads
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2;

    // Select files within the specified range
    std::vector<std::string> selectedFiles;
    {
        std::lock_guard<std::mutex> lock(binImgFilesMutex); // Lock the mutex while accessing binImgFiles
        for (int i = start; i <= end; i++) {
            selectedFiles.push_back(binImgFiles[i - 1]);
        }
    } // Unlock the mutex when leaving the scope

    // Construct the shell-escaped file paths
    std::vector<std::string> escapedSelectedFiles;
    for (const std::string& file : selectedFiles) {
        escapedSelectedFiles.push_back(shell_escape(file));
    }

    // Divide the work among threads
    std::vector<std::thread> threads;
    size_t filesPerThread = escapedSelectedFiles.size() / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        size_t startIdx = i * filesPerThread;
        size_t endIdx = (i == numThreads - 1) ? escapedSelectedFiles.size() : (i + 1) * filesPerThread;
        std::vector<std::string> threadFiles(escapedSelectedFiles.begin() + startIdx, escapedSelectedFiles.begin() + endIdx);
        threads.emplace_back([threadFiles, numThreads]() {
            convertBINsToISOs(threadFiles, numThreads);
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }
}


// Main function to select directories and convert BIN/IMG files to ISO format
void select_and_convert_files_to_iso() {
    // Initialize vectors to store BIN/IMG files and directory paths
    std::vector<std::string> binImgFiles;
    std::vector<std::string> directoryPaths;

    // Declare previousPaths as a static variable
    static std::vector<std::string> previousPaths;

    // Read input for directory paths (allow multiple paths separated by semicolons)
    std::string inputPaths = readInputLine("\033[94mEnter the directory path(s) (if many, separate them with \033[1m\033[93m;\033[0m\033[94m) to search for \033[1m\033[92m.bin \033[94mand \033[1m\033[92m.img\033[94m files, or press Enter to return:\n\033[0m");

    // Use semicolon as a separator to split paths
    std::istringstream iss(inputPaths);
    std::string path;
    while (std::getline(iss, path, ';')) {
        // Trim leading and trailing whitespaces from each path
        size_t start = path.find_first_not_of(" \t");
        size_t end = path.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            directoryPaths.push_back(path.substr(start, end - start + 1));
        }
    }
	
    // Check if directoryPaths is empty
    if (directoryPaths.empty()) {
        return;
    }
    // Flag to check if new files are found
	bool newFilesFound = false;

	// Call the findBinImgFiles function to populate the cache
	binImgFiles = findBinImgFiles(directoryPaths, [&binImgFiles, &newFilesFound](const std::string& fileName, const std::string& filePath) {
    // Your callback logic here, if needed
    newFilesFound = true;
	});

	// Print a message only if no new files are found
	if (!newFilesFound && !binImgFiles.empty()) {
		std::cout << " " << std::endl;
		std::cout << "\033[91mNo new .bin .img file(s) over 10MB found. \033[92m" << binImgFiles.size() << " matching file(s) cached in RAM from previous searches.\033[0m" << std::endl;
		std::cout << " " << std::endl;
		std::cout << "Press enter to continue...";
		std::cin.ignore();
	}

    if (binImgFiles.empty()) {
		std::cout << " " << std::endl;
        std::cout << "\033[91mNo .bin or .img file(s) over 10MB found in the specified path(s) or cached in RAM.\n\033[0m";
        std::cout << " " << std::endl;
        std::cout << "Press enter to continue...";
        std::cin.ignore();
        
    } else {
        while (true) {
            std::system("clear");
            // Print the list of BIN/IMG files
            printFileListBin(binImgFiles);
            
            std::cout << " " << std::endl;
            // Prompt user to choose a file or exit
            char* input = readline("\033[94mChoose BIN/IMG file(s) to convert (e.g., '1-3' '1 2', or press Enter to return):\033[0m ");

            // Break the loop if the user presses Enter
            if (input[0] == '\0') {
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


void printFileListBin(const std::vector<std::string>& fileList) {
    std::cout << "Select file(s) to convert to \033[1m\033[92mISO(s)\033[0m:\n";

    for (std::size_t i = 0; i < fileList.size(); ++i) {
        std::string filename = fileList[i];
        std::size_t lastSlashPos = filename.find_last_of('/');
        std::string path = (lastSlashPos != std::string::npos) ? filename.substr(0, lastSlashPos + 1) : "";
        std::string fileNameOnly = (lastSlashPos != std::string::npos) ? filename.substr(lastSlashPos + 1) : filename;

        std::size_t dotPos = fileNameOnly.find_last_of('.');

        // Check if the file has a ".img" or ".bin" extension
        if (dotPos != std::string::npos &&
            (fileNameOnly.substr(dotPos) == ".img" || fileNameOnly.substr(dotPos) == ".bin")) {
            // Print path in white and filename in green and bold
            std::cout << std::setw(2) << std::right << i + 1 << ". \033[0m" << path << "\033[1m\033[38;5;208m" << fileNameOnly << "\033[0m" << std::endl;
        } else {
            // Print entire path and filename in white
            std::cout << std::setw(2) << std::right << i + 1 << ". \033[0m" << filename << std::endl;
        }
    }
}


// Function to process user input and convert selected BIN files to ISO format
void processInputBin(const std::string& input, const std::vector<std::string>& fileList) {
    // Tokenize the input string
    std::istringstream iss(input);
    std::string token;

    // Vector to store threads for parallel processing
    std::vector<std::thread> threads;

    // Set to keep track of processed indices to avoid duplicate processing
    std::set<int> processedIndices;

    // Vector to store error messages for reporting after conversions
    std::vector<std::string> errorMessages;

    // Iterate over tokens in the input string
    while (iss >> token) {
        // Tokenize each token to check for ranges or single indices
        std::istringstream tokenStream(token);
        int start, end;
        char dash;

        // Check if the token can be converted to an integer
        if (tokenStream >> start) {
            // Check for a range (e.g., 1-5)
            if (tokenStream >> dash && dash == '-' && tokenStream >> end) {
                // Validate the range and create threads for each index in the range
                if (start <= end) {
                    if (start >= 1 && end <= fileList.size()) {
                        int step = 1;
                        for (int i = start; i <= end; i += step) {
                            int selectedIndex = i - 1;
                            // Check if the index has not been processed before
                            if (processedIndices.find(selectedIndex) == processedIndices.end()) {
                                std::string selectedFile = fileList[selectedIndex];
                                // Create a thread for conversion
                                threads.emplace_back(convertBINToISO, selectedFile);
                                // Mark the index as processed
                                processedIndices.insert(selectedIndex);
                            }
                        }
                    } else {
                        // Report an error if the range is out of range
                        errorMessages.push_back("\033[91mInvalid range: '" + std::to_string(start) + "-" + std::to_string(end) + "'. Ensure that numbers align with the list.\033[0m");
                    }
                } else {
                    if (start >= 1 && end >= 1 && end <= fileList.size()) {
                        int step = -1;
                        for (int i = start; i >= end; i += step) {
                            int selectedIndex = i - 1;
                            // Check if the index is within the valid range
                            if (selectedIndex >= 0 && selectedIndex < fileList.size()) {
                                // Check if the index has not been processed before
                                if (processedIndices.find(selectedIndex) == processedIndices.end()) {
                                    std::string selectedFile = fileList[selectedIndex];
                                    // Create a thread for conversion
                                    threads.emplace_back(convertBINToISO, selectedFile);
                                    // Mark the index as processed
                                    processedIndices.insert(selectedIndex);
                                }
                            } else {
                                // Report an error if the index is out of range
                                errorMessages.push_back("\033[91mInvalid range: '" + std::to_string(start) + "-" + std::to_string(end) + "'. Ensure that numbers align with the list.\033[0m");
                                break; // Exit the loop to avoid further errors
                            }
                        }
                    } else {
                        // Report an error if the range is out of range
                        errorMessages.push_back("\033[91mInvalid range: '" + std::to_string(start) + "-" + std::to_string(end) + "'. Ensure that numbers align with the list.\033[0m");
                    }
                }
            } else if (start >= 1 && start <= fileList.size()) {
                // Process a single index
                int selectedIndex = start - 1;
                // Check if the index has not been processed before
                if (processedIndices.find(selectedIndex) == processedIndices.end()) {
                    // Check if the index is within the valid range
                    if (selectedIndex >= 0 && selectedIndex < fileList.size()) {
                        std::string selectedFile = fileList[selectedIndex];
                        // Create a thread for conversion
                        threads.emplace_back(convertBINToISO, selectedFile);
                        // Mark the index as processed
                        processedIndices.insert(selectedIndex);
                    } else {
                        // Report an error if the index is out of range
                        errorMessages.push_back("\033[91mFile index '" + std::to_string(start) + "' does not exist.\033[0m");
                    }
                }
            } else {
                // Report an error if the index is out of range
                errorMessages.push_back("\033[91mFile index '" + std::to_string(start) + "' does not exist.\033[0m");
            }
        } else {
            // Report an error if the token is not a valid integer
            errorMessages.push_back("\033[91mInvalid input: '" + token + "'.\033[0m");
        }
    }
    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Print all error messages after conversions
    for (const auto& errorMessage : errorMessages) {
        std::cout << errorMessage << std::endl;
    }
    std::cout << " " << std::endl;
}


// Function to search for .mdf and .mds files under 10MB
std::vector<std::string> findMdsMdfFiles(const std::vector<std::string>& paths, const std::function<void(const std::string&, const std::string&)>& callback) {
    // Vector to store cached invalid paths
    static std::vector<std::string> cachedInvalidPaths;

    // Static variables to cache results for reuse
    static std::vector<std::string> mdfMdsFilesCache;

    // Vector to store file names that match the criteria
    std::vector<std::string> fileNames;

    // Clear the cachedInvalidPaths before processing a new set of paths
    cachedInvalidPaths.clear();

    bool printedEmptyLine = false;  // Flag to track if an empty line has been printed

    try {
        // Mutex to ensure thread safety
        static std::mutex mutex;

        // Determine the maximum number of threads to use based on hardware concurrency; fallback is 2 threads
        const int maxThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2;

        // Use a thread pool for parallel processing
        std::vector<std::future<void>> futures;

        // Iterate through input paths
        for (const auto& path : paths) {
            try {
                // Use a lambda function to process files asynchronously
                auto processFileAsync = [&](const std::filesystem::directory_entry& entry) {
                    std::string fileName = entry.path().string();
                    std::string filePath = entry.path().parent_path().string();  // Get the path of the directory

                    // Call the callback function to inform about the found file
                    callback(fileName, filePath);

                    // Lock the mutex to ensure safe access to shared data (fileNames)
                    std::lock_guard<std::mutex> lock(mutex);
                    fileNames.push_back(fileName);
                };

                // Use thread pool to process files concurrently
                futures.clear();

                // Iterate through files in the given directory and its subdirectories
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        // Check if the file has a ".mdf" or ".mds" extension and is larger than or equal to 10,000,000 bytes
                        std::string ext = entry.path().extension();
                        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
                            return std::tolower(c);
                        });

                        if ((ext == ".mdf" || ext == ".mds") && std::filesystem::file_size(entry) >= 10'000'000) {
                            // Check if the file is already present in the cache to avoid duplicates
                            std::string fileName = entry.path().string();
                            if (std::find(mdfMdsFilesCache.begin(), mdfMdsFilesCache.end(), fileName) == mdfMdsFilesCache.end()) {
                                // Process the file asynchronously using the thread pool
                                futures.emplace_back(std::async(std::launch::async, processFileAsync, entry));
                            }
                        }
                    }
                }

                // Wait for all asynchronous tasks to complete
                for (auto& future : futures) {
                    future.get();
                }

            } catch (const std::filesystem::filesystem_error& e) {
                if (!printedEmptyLine) {
                    // Print an empty line before starting to print invalid paths (only once)
                    std::cout << " " << std::endl;
                    printedEmptyLine = true;
                }
                // Handle filesystem errors for the current directory
                if (std::find(cachedInvalidPaths.begin(), cachedInvalidPaths.end(), path) == cachedInvalidPaths.end()) {
                    std::cerr << "\033[91mInvalid directory path: '" << path << "'. Excluded from search." << "\033[0m" << std::endl;
                    // Add the invalid path to cachedInvalidPaths to avoid duplicate error messages
                    cachedInvalidPaths.push_back(path);
                }
            }
        }

    } catch (const std::filesystem::filesystem_error& e) {
        if (!printedEmptyLine) {
            // Print an empty line before starting to print invalid paths (only once)
            std::cout << " " << std::endl;
            printedEmptyLine = true;
        }
        // Handle filesystem errors for the overall operation
        std::cerr << "\033[91m" << e.what() << "\033[0m" << std::endl;
        std::cin.ignore();
    }

    // Print success message if files were found
    if (!fileNames.empty()) {
        std::cout << " " << std::endl;
        std::cout << "\033[92mFound " << fileNames.size() << " matching file(s)\033[0m" << ".\033[93m " << mdfMdsFilesCache.size() << " matching file(s) cached in RAM from previous searches.\033[0m" << std::endl;
        std::cout << " " << std::endl;
        std::cout << "Press enter to continue...";
        std::cin.ignore();
    }

    // Remove duplicates from fileNames by sorting and using unique erase idiom
    std::sort(fileNames.begin(), fileNames.end());
    fileNames.erase(std::unique(fileNames.begin(), fileNames.end()), fileNames.end());

    // Update the cache by appending fileNames to mdfMdsFilesCache
    mdfMdsFilesCache.insert(mdfMdsFilesCache.end(), fileNames.begin(), fileNames.end());

    // Return the combined results
    return mdfMdsFilesCache;
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
        std::cout << "\033[91mThe specified input file \033[93m'" << inputPath << "'\033[91m does not exist.\033[0m" << std::endl;
        return;
    }

    // Check if the corresponding .iso file already exists
    std::string isoOutputPath = inputPath.substr(0, inputPath.find_last_of(".")) + ".iso";
    if (std::ifstream(isoOutputPath)) {
        std::cout << "\033[93mThe corresponding .iso file already exists for: \033[92m'" << inputPath << "'\033[93m. Skipping conversion.\033[0m" << std::endl;
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
        std::cout << "\033[91mFailed to execute conversion command\033[0m" << std::endl;
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
            std::cout << "\033[91mThe selected file \033[93m'" << inputPath << "'\033[91m is already in ISO format, maybe rename it to .iso?. Skipping conversion.\033[0m" << std::endl;
        } else {
            std::cout << "Image file converted to ISO: \033[92m'" << outputPath << "'\033[0m." << std::endl;
        }
    } else {
        std::cout << "\033[91mConversion of \033[93m'" << inputPath << "'\033[91m failed.\033[0m" << std::endl;
    }
}

// Function to convert multiple MDF files to ISO format using mdf2iso
void convertMDFsToISOs(const std::vector<std::string>& inputPaths) {
    // Check if mdf2iso is installed
    if (!isMdf2IsoInstalled()) {
        std::cout << "\033[91mmdf2iso is not installed. Please install it before using this option.\033[0m";
        return;
    }

    // Determine the number of threads based on hardware concurrency, fallback is 2 threads
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads <= 0) {
        // Fallback to a default number of threads if hardware concurrency is not available
        numThreads = 2;
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

// Function to process a range of MDF files by converting them to ISO asynchronously
void processMDFFilesInRange(int start, int end) {
    std::vector<std::string> mdfImgFiles; // Declare mdfImgFiles here
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2; // Determine the number of threads based on CPU cores fallback is 2 threads
    std::vector<std::string> selectedFiles;

    {
        std::lock_guard<std::mutex> lock(mdfFilesMutex); // Lock the mutex while accessing mdfImgFiles
        for (int i = start; i <= end; i++) {
            std::string filePath = (mdfImgFiles[i - 1]);
            selectedFiles.push_back(filePath);
        }
    } // Unlock the mutex when leaving the scope

    int filesPerThread = selectedFiles.size() / numThreads;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < numThreads; ++i) {
        int startIdx = i * filesPerThread;
        int endIdx = (i == numThreads - 1) ? selectedFiles.size() : (i + 1) * filesPerThread;
        std::vector<std::string> filesSubset(selectedFiles.begin() + startIdx, selectedFiles.begin() + endIdx);

        // Use std::async for asynchronous execution
        futures.emplace_back(std::async(std::launch::async, [filesSubset]() {
            convertMDFsToISOs(filesSubset);
        }));
    }

    // Wait for all asynchronous tasks to finish
    for (auto& future : futures) {
        future.wait();
    }
}

// Function to interactively select and convert MDF files to ISO
void select_and_convert_files_to_iso_mdf() {
    // Read input for directory paths (allow multiple paths separated by semicolons)
    std::string inputPaths = readInputLine("\033[94mEnter the directory path(s) (if many, separate them with \033[1m\033[93m;\033[0m\033[94m) to search for \033[1m\033[92m.mdf\033[94m files, or press Enter to return:\n\033[0m");

    // Initialize vectors to store MDF/MDS files and directory paths
    std::vector<std::string> mdfMdsFiles;
    std::vector<std::string> directoryPaths;

    // Declare previousPaths as a static variable
    static std::vector<std::string> previousPaths;

    // Use semicolon as a separator to split paths
    std::istringstream iss(inputPaths);
    std::string path;
    while (std::getline(iss, path, ';')) {
        // Trim leading and trailing whitespaces from each path
        size_t start = path.find_first_not_of(" \t");
        size_t end = path.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            directoryPaths.push_back(path.substr(start, end - start + 1));
        }
    }

    // Check if directoryPaths is empty
    if (directoryPaths.empty()) {
        return;
    }

    // Flag to check if new .mdf files are found
    bool newMdfFilesFound = false;

    // Call the findMdsMdfFiles function to populate the cache
    mdfMdsFiles = findMdsMdfFiles(directoryPaths, [&mdfMdsFiles, &newMdfFilesFound](const std::string& fileName, const std::string& filePath) {
        newMdfFilesFound = true;
    });

    // Print a message only if no new .mdf files are found
    if (!newMdfFilesFound && !mdfMdsFiles.empty()) {
        std::cout << " " << std::endl;
        std::cout << "\033[91mNo new .mdf file(s) over 10MB found. \033[92m" << mdfMdsFiles.size() << " file(s) cached in RAM from previous searches.\033[0m" << std::endl;
        std::cout << " " << std::endl;
        std::cout << "Press enter to continue...";
        std::cin.ignore();
    }

    if (mdfMdsFiles.empty()) {
        std::cout << " " << std::endl;
        std::cout << "\033[91mNo .mdf file(s) over 10MB found in the specified path(s) or cached in RAM.\n\033[0m";
        std::cout << " " << std::endl;
        std::cout << "Press enter to continue...";
        std::cin.ignore();
        return;
    }

    // Continue selecting and converting files until the user decides to exit
    while (true) {
        std::system("clear");
        printFileListMdf(mdfMdsFiles);

        std::cout << " " << std::endl;
        // Prompt the user to enter file numbers or 'exit'
        char* input = readline("\033[94mChoose MDF file(s) to convert (e.g., '1-2' or '1 2', or press Enter to return):\033[0m ");

        if (input[0] == '\0') {
            std::system("clear");
            break;
        }

        // Parse the user input to get selected file indices and capture errors
        std::pair<std::vector<int>, std::vector<std::string>> result = parseUserInput(input, mdfMdsFiles.size());
        std::vector<int> selectedFileIndices = result.first;
        std::vector<std::string> errorMessages = result.second;
        std::system("clear");

        if (!selectedFileIndices.empty()) {
            // Get the paths of the selected files based on user input
            std::future<std::vector<std::string>> futureSelectedFiles = getSelectedFiles(selectedFileIndices, mdfMdsFiles);

            // Wait for the asynchronous task to complete and retrieve the result
            std::vector<std::string> selectedFiles = futureSelectedFiles.get();

            // Convert the selected MDF files to ISO
            convertMDFsToISOs(selectedFiles);

            // Display errors if any
            for (const auto& errorMessage : errorMessages) {
                std::cerr << errorMessage << std::endl;
            }
            std::cout << " " << std::endl;
            std::cout << "Press enter to continue...";
            std::cin.ignore();
        } else {
            // Display parsing errors
            for (const auto& errorMessage : errorMessages) {
                std::cerr << errorMessage << std::endl;
            }
            std::cout << " " << std::endl;
            std::cout << "Press enter to continue...";
            std::cin.ignore();
        }
    }
}


void printFileListMdf(const std::vector<std::string>& fileList) {
    std::cout << "Select file(s) to convert to \033[1m\033[92mISO(s)\033[0m:\n";

    for (std::size_t i = 0; i < fileList.size(); ++i) {
        std::string filename = fileList[i];
        std::size_t lastSlashPos = filename.find_last_of('/');
        std::string path = (lastSlashPos != std::string::npos) ? filename.substr(0, lastSlashPos + 1) : "";
        std::string fileNameOnly = (lastSlashPos != std::string::npos) ? filename.substr(lastSlashPos + 1) : filename;

        std::size_t dotPos = fileNameOnly.find_last_of('.');

        // Check if the file has a ".mdf" extension
        if (dotPos != std::string::npos && fileNameOnly.substr(dotPos) == ".mdf") {
            // Print path in white and filename in orange and bold
            std::cout << std::setw(2) << std::right << i + 1 << ". \033[0m" << path << "\033[1m\033[38;5;208m" << fileNameOnly << "\033[0m" << std::endl;
        } else {
            // Print entire path and filename in white
            std::cout << std::setw(2) << std::right << i + 1 << ". \033[0m" << filename << std::endl;
        }
    }
}

// Function to parse user input and extract selected file indices and errors
std::pair<std::vector<int>, std::vector<std::string>> parseUserInput(const std::string& input, int maxIndex) {
    std::vector<int> selectedFileIndices;
    std::vector<std::string> errorMessages;
    std::istringstream iss(input);
    std::string token;

    // Set to track processed indices
    std::set<int> processedIndices;

    // Iterate through the tokens in the input string
    while (iss >> token) {
        if (token.find('-') != std::string::npos) {
            // Handle a range (e.g., "1-2" or "2-1")
            size_t dashPos = token.find('-');
            int startRange, endRange;

            try {
                startRange = std::stoi(token.substr(0, dashPos));
                endRange = std::stoi(token.substr(dashPos + 1));
            } catch (const std::invalid_argument& e) {
                errorMessages.push_back("\033[91mInvalid input " + token + ".\033[0m");
                continue;
            } catch (const std::out_of_range& e) {
                errorMessages.push_back("\033[91mInvalid input " + token + ".\033[0m");
                continue;
            }

            // Check if the range is valid before adding indices
            if ((startRange >= 1 && startRange <= maxIndex) && (endRange >= 1 && endRange <= maxIndex)) {
                int step = (startRange <= endRange) ? 1 : -1;
                for (int i = startRange; (startRange <= endRange) ? (i <= endRange) : (i >= endRange); i += step) {
                    int currentIndex = i - 1;

                    // Check if the index has already been processed
                    if (processedIndices.find(currentIndex) == processedIndices.end()) {
                        selectedFileIndices.push_back(currentIndex);
                        processedIndices.insert(currentIndex);
                    }
                }
            } else {
                errorMessages.push_back("\033[91mInvalid range: '" + token + "'. Ensure that numbers align with the list.\033[0m");
            }
        } else {
            // Handle individual numbers (e.g., "1")
            int selectedFileIndex;

            try {
                selectedFileIndex = std::stoi(token);
            } catch (const std::invalid_argument& e) {
                errorMessages.push_back("\033[91mInvalid input: '" + token + "'.\033[0m");
                continue;
            } catch (const std::out_of_range& e) {
                errorMessages.push_back("\033[91mFile index '" + token + "', does not exist.\033[0m");
                continue;
            }

            // Add the index to the selected indices vector if it is within the valid range
            if (selectedFileIndex >= 1 && selectedFileIndex <= maxIndex) {
                int currentIndex = selectedFileIndex - 1;

                // Check if the index has already been processed
                if (processedIndices.find(currentIndex) == processedIndices.end()) {
                    selectedFileIndices.push_back(currentIndex);
                    processedIndices.insert(currentIndex);
                }
            } else {
                errorMessages.push_back("\033[91mFile index '" + token + "', does not exist.\033[0m");
            }
        }
    }

    return {selectedFileIndices, errorMessages};
}

// Multithreaded function to parse user input and extract selected file indices and errors
std::vector<std::future<std::pair<std::vector<int>, std::vector<std::string>>>> parseUserInputMultithreaded(const std::vector<std::string>& inputs, int maxIndex) {

    std::vector<std::future<std::pair<std::vector<int>, std::vector<std::string>>>> futures;

    // Use std::async to perform user input parsing concurrently
    for (const auto& input : inputs) {
        futures.push_back(std::async(std::launch::async, parseUserInput, input, maxIndex));
    }

    return futures;
}


// Function to retrieve selected files based on their indices asynchronously
std::future<std::vector<std::string>> getSelectedFiles(const std::vector<int>& selectedIndices, const std::vector<std::string>& fileList) {
    // Use std::async with launch policy to create asynchronous tasks
    return std::async(std::launch::async | std::launch::deferred, [selectedIndices, fileList]() {
        std::vector<std::string> selectedFiles;
        std::mutex mtx; // Mutex to protect access to the selectedFiles vector

        // Function to be executed by each asynchronous task
        auto processIndex = [&](int index) {
            std::string file;
            // Check if the index is valid
            if (index >= 0 && index < fileList.size()) {
                file = fileList[index];
            }

            // Lock the mutex before modifying the selectedFiles vector
            std::lock_guard<std::mutex> lock(mtx);
            selectedFiles.push_back(file);
        };

        // Create a vector of asynchronous tasks
        std::vector<std::future<void>> futures;

        // Iterate through the selected indices and create a task for each index
        for (int index : selectedIndices) {
            // Use std::async for asynchronous execution
            futures.emplace_back(std::async(std::launch::async | std::launch::deferred, processIndex, index));
        }

        // Wait for all asynchronous tasks to finish
        for (auto& future : futures) {
            future.wait();
        }

        return selectedFiles;
    });
}
