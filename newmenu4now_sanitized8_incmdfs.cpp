#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <condition_variable>
#include <readline/readline.h>
#include <algorithm>
#include <filesystem>
#include <future>
#include <sys/types.h>
#include <sys/stat.h>
#include <future>
#include <unistd.h>
#include <dirent.h>
#include <sstream>
#include <thread>
#include <queue>
#include <string_view>
#include <map>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <regex>


//	SANITISATION AND STRING STUFF	//

std::string shell_escape(const std::string& param) {
    std::string result = "";
    result.reserve(param.size() + 2); // Reserve space for single quotes

    result += '\''; // Start with an opening single quote

    for (char c : param) {
        if (c == '\'') {
            // Escape single quotes by adding an additional one
            result += "''";
        } else {
            // Just append the character
            result += c;
        }
    }

    result += '\''; // End with a closing single quote

    return result;
}


// Function to read a line of input using readline
std::string readInputLine(const std::string& prompt) {
    char* input = readline(prompt.c_str());

    if (input) {
        std::string inputStr(input);
        free(input); // Free the allocated memory for the input
        return inputStr;
    }

    return ""; // Return an empty string if readline fails
}

// MULTITHREADING STUFF
std::mutex mountMutex; // Mutex for thread safety
std::mutex mtx;

// Define the default cache directory
const std::string cacheDirectory = "/tmp/";

namespace fs = std::filesystem;


//	Function prototypes	//

//	stds
std::vector<std::string> findBinImgFiles(const std::string& directory);
std::vector<std::string> findMdsMdfFiles(const std::string& directory);
std::string chooseFileToConvert(const std::vector<std::string>& files);
//	bools
bool directoryExists(const std::string& path);
bool hasIsoExtension(const std::string& filePath);
bool iequals(const std::string& a, const std::string& b);
bool isMdf2IsoInstalled();

//	voids
void convertMDFToISO(const std::string& inputPath);
void convertMDFsToISOs(const std::vector<std::string>& inputPaths, int numThreads);
void processMDFFilesInRange(int start, int end);
void select_and_convert_files_to_iso_mdf();
void processMdfMdsFilesInRange(const std::vector<std::string>& mdfMdsFiles, int start, int end);
void mountISO(const std::vector<std::string>& isoFiles);
void listAndMountISOs();
void unmountISOs();
void cleanAndUnmountAllISOs();
void convertBINsToISOs();
void listMountedISOs();
void listMode();
void manualMode_isos();
void select_and_mount_files_by_number();
void select_and_convert_files_to_iso();
void manualMode_imgs();
void print_ascii();
void screen_clear();
void print_ascii();
void traverseDirectory(const std::filesystem::path& path, std::vector<std::string>& isoFiles);
void parallelTraverse(const std::filesystem::path& path, std::vector<std::string>& isoFiles);


std::string directoryPath;				// Declare directoryPath here
std::vector<std::string> binImgFiles;	// Declare binImgFiles here
std::vector<std::string> mdfImgFiles;	// Declare mdfImgFiles here



int main() {
    std::string choice;

    while (true) {
			
			std::system("clear");
			print_ascii();
        // Display the main menu options
        std::cout << "Menu Options:" << std::endl;
        std::cout << "1. List and Mount ISOs" << std::endl;
        std::cout << "2. Unmount ISOs" << std::endl;
        std::cout << "3. Clean and Unmount All ISOs" << std::endl;
        std::cout << "4. Conversion Tools" << std::endl;
        std::cout << "5. List Mounted ISOs" << std::endl;
        std::cout << "6. Exit the Program" << std::endl;
         // Prompt for the main menu choice
        std::cout << "\033[32mEnter your choice:\033[0m ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // Consume the newline character

        if (choice == "1") {
            select_and_mount_files_by_number();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::system("clear");
        } else if (choice == "2") {
            // Call your unmountISOs function
            unmountISOs();
            std::cout << "Press Enter to continue...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::system("clear");
        } else if (choice == "3") {
            // Call your cleanAndUnmountAllISOs function
            cleanAndUnmountAllISOs();
            std::cout << "Press Enter to continue...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::system("clear");
            } else if (choice == "4") {
            while (true) {
                // Display the submenu for choice 4
                std::cout << "Convert Files to ISO:" << std::endl;
                std::cout << "1. Convert to ISO (BIN2ISO)" << std::endl;
                std::cout << "2. Convert to ISO (MDF2ISO)" << std::endl;
                std::cout << "3. Back to Main Menu" << std::endl;

                // Prompt for the submenu choice
                std::cout << "\033[32Enter your choice:\033[0m ";
                std::cin >> choice;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // Consume the newline character

                if (choice == "1") {
                    select_and_convert_files_to_iso();
                } else if (choice == "2") {
                    select_and_convert_files_to_iso_mdf();
                } else if (choice == "3") {
                    break;  // Go back to the main menu
                } else {
                    std::cout << "\033[31mInvalid choice. Please enter 1, 2, or 3.\033[0m" << std::endl;
                }
            }
        } else if (choice == "5") {
            // Call your listMountedISOs function
            listMountedISOs();
            std::cout << "Press Enter to continue...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::system("clear");
        } else if (choice == "6") {
            std::cout << "Exiting the program..." << std::endl;
            return 0;
        } else {
            std::cout << "\033[31mInvalid choice. Please enter 1, 2, 3, 4, 5, or 6.\033[0m" << std::endl;
        }
    }

    return 0;
}

// ... Function definitions ...


void print_ascii() {
    /// Display ASCII art

std::cout << "\033[32m  _____          ___ _____ _____   ___ __   __  ___  _____ _____   __   __  ___ __   __ _   _ _____ _____ ____          _   ___   ___             \033[0m" << std::endl;
std::cout << "\033[32m |  ___)   /\\   (   |_   _)  ___) (   )  \\ /  |/ _ \\|  ___)  ___) |  \\ /  |/ _ (_ \\ / _) \\ | (_   _)  ___)  _ \\        / | /   \\ / _ \\  \033[0m" << std::endl;
std::cout << "\033[32m | |_     /  \\   | |  | | | |_     | ||   v   | |_| | |   | |_    |   v   | | | |\\ v / |  \\| | | | | |_  | |_) )  _  __- | \\ O /| | | |      \033[0m" << std::endl;
std::cout << "\033[32m |  _)   / /\\ \\  | |  | | |  _)    | || |\\_/| |  _  | |   |  _)   | |\\_/| | | | | | |  |     | | | |  _) |  __/  | |/ /| | / _ \\| | | |     \033[0m" << std::endl;
std::cout << "\033[32m | |___ / /  \\ \\ | |  | | | |___   | || |   | | | | | |   | |___  | |   | | |_| | | |  | |\\  | | | | |___| |     | / / | |( (_) ) |_| |       \033[0m" << std::endl;
std::cout << "\033[32m |_____)_/    \\_(___) |_| |_____) (___)_|   |_|_| |_|_|   |_____) |_|   |_|\\___/  |_|  |_| \\_| |_| |_____)_|     |__/  |_(_)___/ \\___/       \033[0m" << std::endl;
		
std::cout << " " << std::endl;
}


// MOUNT FUNCTIONS


bool directoryExists(const std::string& path) {
    return fs::is_directory(path);
}

void mountIsoFile(const std::string& isoFile, std::map<std::string, std::string>& mountedIsos) {
    // Check if the ISO file is already mounted
    std::lock_guard<std::mutex> lock(mountMutex); // Lock to protect access to mountedIsos
    if (mountedIsos.find(isoFile) != mountedIsos.end()) {
        std::cout << "\e[1;31mALREADY MOUNTED\e[0m: ISO file '" << isoFile << "' is already mounted at '" << mountedIsos[isoFile] << "'." << std::endl;
        return;
    }

    // Use the filesystem library to extract the ISO file name
    fs::path isoPath(isoFile);
    std::string isoFileName = isoPath.stem().string(); // Remove the .iso extension

    std::string mountPoint = "/mnt/iso_" + isoFileName; // Use the modified ISO file name in the mount point with "iso_" prefix

    // Check if the mount point directory doesn't exist, create it
    if (!directoryExists(mountPoint)) {
        // Create the mount point directory
        std::string mkdirCommand = "sudo mkdir -p " + shell_escape(mountPoint);
        if (system(mkdirCommand.c_str()) != 0) {
            std::perror("\033[33mFailed to create mount point directory\033[0m");
            return;
        }

        // Mount the ISO file to the mount point
        std::string mountCommand = "sudo mount -o loop " + shell_escape(isoFile) + " " + shell_escape(mountPoint);
        if (system(mountCommand.c_str()) != 0) {
            std::perror("\033[31mFailed to mount ISO file\033[0m");
        } else {
            std::cout << "ISO file '" << isoFile << "' mounted at '" << mountPoint << "'." << std::endl;
            // Store the mount point in the map
            mountedIsos[isoFile] = mountPoint;
        }
    } else {
        // The mount point directory already exists, so the ISO is considered mounted
        //std::cout << "ISO file '" << isoFile << "' is already mounted at '" << mountPoint << "'." << std::endl;
        //mountedIsos[isoFile] = mountPoint;
        //std::cout << "Press Enter to continue...";
       // std::cin.get(); // Wait for the user to press Enter
    }
}

void mountISO(const std::vector<std::string>& isoFiles) {
    std::map<std::string, std::string> mountedIsos;
    int count = 1;
    std::vector<std::thread> threads;

    for (const std::string& isoFile : isoFiles) {
        // Limit the number of threads to a maximum of 4
        if (threads.size() >= 4) {
            for (auto& thread : threads) {
                thread.join();
            }
            threads.clear();
        }

        // Shell-escape the ISO file path before mounting
        std::string escapedIsoFile = (isoFile);
        threads.emplace_back(mountIsoFile, escapedIsoFile, std::ref(mountedIsos));
    }

    // Join any remaining threads
    for (auto& thread : threads) {
        thread.join();
    }

    system("clear");
    // Print a message indicating that all ISO files have been mounted
    std::cout << "\e[1;32mPreviously Selected ISO files have been mounted.\n\e[0m" << std::endl;
}
void select_and_mount_files_by_number() {
    std::string directoryPath = readInputLine("\033[32mEnter the directory path to search for .iso files:\033[0m ");

    if (directoryPath.empty()) {
        std::cout << "\033[33mPath input is empty. Exiting.\033[0m" << std::endl;
        return;
    }

    std::vector<std::string> isoFiles;
    traverseDirectory(directoryPath, isoFiles);

    std::vector<std::string> mountedISOs;

    while (true) {
        if (isoFiles.empty()) {
            std::cout << "\033[33mNo .iso files found in the specified directory and its subdirectories.\033[0m" << std::endl;
            break;
        }

        // Remove already mounted files from the selection list
        for (const auto& mountedISO : mountedISOs) {
            auto it = std::remove(isoFiles.begin(), isoFiles.end(), mountedISO);
            isoFiles.erase(it, isoFiles.end());
        }

        if (isoFiles.empty()) {
            std::cout << "\033[33mNo more unmounted .iso files in the directory.\033[33m" << std::endl;
            break;
        }

        for (int i = 0; i < isoFiles.size(); i++) {
            std::cout << i + 1 << ". " << isoFiles[i] << std::endl;
        }

        std::string input;
        std::cout << "\033[32mChoose .iso files to mount (enter numbers separated by spaces or ranges like '1-3', or press Enter to exit):\033[0m ";
        std::getline(std::cin, input);

        if (input.empty()) {
            std::cout << "Press Enter to Return" << std::endl;
            break;
        }

        std::istringstream iss(input);
        std::string token;

        while (std::getline(iss, token, ' ')) {
            if (token.find('-') != std::string::npos) {
                // Handle a range
                size_t dashPos = token.find('-');
                int startRange = std::stoi(token.substr(0, dashPos));
                int endRange = std::stoi(token.substr(dashPos + 1));

                if (startRange >= 1 && startRange <= isoFiles.size() && endRange >= startRange && endRange <= isoFiles.size()) {
                    for (int i = startRange; i <= endRange; i++) {
                        int selectedNumber = i;
                        std::string selectedISO = isoFiles[selectedNumber - 1];

                        // Check if the selected ISO is not already mounted
                        if (std::find(mountedISOs.begin(), mountedISOs.end(), selectedISO) == mountedISOs.end()) {
                            // Shell-escape the selected ISO file path before mounting
                            mountISO({(selectedISO)});
                            mountedISOs.push_back(selectedISO);
                        } else {
                            std::cout << "\033[33mISO file '" << selectedISO << "' is already mounted.\033[0m" << std::endl;
                        }
                    }
                } else {
                    std::cout << "\033[31mInvalid range: " << token << ". Please try again.\033[0m" << std::endl;
                }
            } else {
                int selectedNumber = std::stoi(token);
                if (selectedNumber >= 1 && selectedNumber <= isoFiles.size()) {
                    std::string selectedISO = isoFiles[selectedNumber - 1];

                    // Check if the selected ISO is not already mounted
                    if (std::find(mountedISOs.begin(), mountedISOs.end(), selectedISO) == mountedISOs.end()) {
                        // Shell-escape the selected ISO file path before mounting
                        mountISO({(selectedISO)});
                        mountedISOs.push_back(selectedISO);
                    } else {
                        std::cout << "\033[33mISO file '" << selectedISO << "' is already mounted.\033[0m" << std::endl;
                    }
                } else {
                    std::cout << "\033[31mInvalid number: " << selectedNumber << ". Please try again.\033[30m" << std::endl;
                }
            }
        }
    }
}

bool iequals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char lhs, char rhs) {
        return std::tolower(lhs) == std::tolower(rhs);
    });
}

void traverseDirectory(const std::filesystem::path& path, std::vector<std::string>& isoFiles) {
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                const std::filesystem::path& filePath = entry.path();

                std::string extensionStr = filePath.extension().string();
                std::string_view extension = extensionStr;

                if (iequals(std::string(extension), ".iso")) {
                    isoFiles.push_back(filePath.string());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void parallelTraverse(const std::filesystem::path& path, std::vector<std::string>& isoFiles) {
    int numThreads = std::thread::hardware_concurrency();

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&path, &isoFiles]() {
            traverseDirectory(path, isoFiles);
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }
}




// Function to check if a file has the .iso extension
bool hasIsoExtension(const std::string& filePath) {
    // Extract the file extension and check if it's ".iso"
    size_t pos = filePath.find_last_of('.');
    if (pos != std::string::npos) {
        std::string extension = filePath.substr(pos);

        // Shell-escape the extension for robustness
        std::string escapedExtension = shell_escape(extension);

        return escapedExtension == ".iso";
    }
    return false;
}



bool hasIsoExtensionInParallel(const std::vector<std::string>& filePaths) {
    std::vector<bool> results(filePaths.size(), false);

    // Define a function to be executed by each thread
    auto checkIsoExtension = [&](int start, int end) {
        for (int i = start; i < end; i++) {
            results[i] = hasIsoExtension(filePaths[i]);
        }
    };

    // Create four threads to perform the checks in parallel (for a maximum of 4 cores)
    int numThreads = std::min(4, static_cast<int>(filePaths.size()));
    std::vector<std::thread> threads;

    int batchSize = filePaths.size() / numThreads;
    for (int i = 0; i < numThreads; i++) {
        int start = i * batchSize;
        int end = (i == numThreads - 1) ? filePaths.size() : (i + 1) * batchSize;
        
        // Create a copy of the file path with shell escape
        std::vector<std::string> escapedFilePaths;
        for (int j = start; j < end; j++) {
            escapedFilePaths.push_back(shell_escape(filePaths[j]));
        }

        threads.emplace_back([escapedFilePaths, &results, start, end] {
            for (int j = start; j < end; j++) {
                results[j] = hasIsoExtension(escapedFilePaths[j - start]);
            }
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Check if any of the threads found an ".iso" file
    for (bool result : results) {
        if (result) {
            return true;
        }
    }

    return false;
}

// UMOUNT FUNCTIONS

void unmountISOs() {
    const std::string isoPath = "/mnt";

    while (true) {
        std::vector<std::string> isoDirs;

        // Find and store directories with the name "iso_*" in /mnt
        DIR* dir;
        struct dirent* entry;

        if ((dir = opendir(isoPath.c_str())) != NULL) {
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_DIR && std::string(entry->d_name).find("iso_") == 0) {
                    isoDirs.push_back(isoPath + "/" + entry->d_name);
                }
            }
            closedir(dir);
        }

        if (isoDirs.empty()) {
            std::cout << "\033[33mNO ISOS MOUNTED, NOTHING TO DO.\n\033[0m";
            return;
        }

        // Display a list of mounted ISOs with indices
        std::cout << "\033[32mList of mounted ISOs:\033[0m" << std::endl;
        for (size_t i = 0; i < isoDirs.size(); ++i) {
            std::cout << i + 1 << ". " << isoDirs[i] << std::endl;
        }

        // Prompt for unmounting input
        std::cout << "\033[33mEnter the indices of ISOs to unmount (e.g., 1, 2, 1-2) or type enter to exit:\033[0m ";
        std::string input;
        std::getline(std::cin, input);
        std::system("clear");

        if (input == "") {
            std::cout << "Exiting the unmounting tool." << std::endl;
            break;  // Exit the loop
        }

        // Continue with the rest of the logic to process user input
        std::istringstream iss(input);
        std::vector<int> unmountIndices;
        std::string token;

        while (iss >> token) {
            std::smatch match;
            std::regex range_regex("(\\d+)-(\\d+)");
            if (std::regex_match(token, match, range_regex)) {
                int startRange = std::stoi(match[1]);
                int endRange = std::stoi(match[2]);
                if (startRange >= 1 && endRange >= startRange && static_cast<size_t>(endRange) <= isoDirs.size()) {
                    for (int i = startRange; i <= endRange; ++i) {
                        unmountIndices.push_back(i);
                    }
                } else {
                    std::cerr << "\033[31mInvalid range. Please try again.\n\033[0m" << std::endl;
                }
            } else {
                int index = std::stoi(token);
                if (index >= 1 && static_cast<size_t>(index) <= isoDirs.size()) {
                    unmountIndices.push_back(index);
                } else {
                    std::cerr << "\033[31mInvalid index. Please try again.\n\033[0m" << std::endl;
                }
            }
        }

        if (unmountIndices.empty()) {
            std::cerr << "\033[31mNo valid indices provided. Please try again.\n\033[0m" << std::endl;
            continue;  // Restart the loop
        }

        // Unmount and attempt to remove the selected ISOs, suppressing output
        for (int index : unmountIndices) {
            const std::string& isoDir = isoDirs[index - 1];

            // Unmount the ISO and suppress logs
            std::string unmountCommand = "sudo umount -l " + shell_escape(isoDir) + " > /dev/null 2>&1";
            int result = system(unmountCommand.c_str());

            if (result == 0) {
                // Omitted log for unmounting
            } else {
                // Omitted log for unmounting
            }

            // Remove the directory, regardless of unmount success, and suppress error message
            std::string removeDirCommand = "sudo rmdir -p " + shell_escape(isoDir) + " 2>/dev/null";
            int removeDirResult = system(removeDirCommand.c_str());

            if (removeDirResult == 0) {
                // Omitted log for directory removal
            }
        }
    }
}



void unmountAndCleanISO(const std::string& isoDir) {
    std::string unmountCommand = "sudo umount -l " + shell_escape(isoDir) + " 2>/dev/null";
    int result = std::system(unmountCommand.c_str());

    // if (result != 0) {
    //     std::cerr << "Failed to unmount " << isoDir << " with sudo." << std::endl;
    // }

    // Remove the directory after unmounting
    std::string removeDirCommand = "sudo rmdir " + shell_escape(isoDir);
    int removeDirResult = std::system(removeDirCommand.c_str());

    if (removeDirResult != 0) {
        std::cerr << "\033[31mFailed to remove directory\033[0m" << isoDir << std::endl;
    }
}

void cleanAndUnmountISO(const std::string& isoDir) {
    std::lock_guard<std::mutex> lock(mtx);

    // Construct a shell-escaped ISO directory path
    std::string escapedIsoDir = shell_escape(isoDir);

    unmountAndCleanISO(escapedIsoDir);
}

void cleanAndUnmountAllISOs() {
    std::cout << "\n";
    std::cout << "Clean and Unmount All ISOs function." << std::endl;
    const std::string isoPath = "/mnt";
    std::vector<std::string> isoDirs;

    DIR* dir;
    struct dirent* entry;

    if ((dir = opendir(isoPath.c_str())) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR && std::string(entry->d_name).find("iso_") == 0) {
                isoDirs.push_back(isoPath + "/" + entry->d_name);
            }
        }
        closedir(dir);
    }

    if (isoDirs.empty()) {
        std::cout << "\033[33mNO ISOS LEFT TO BE CLEANED\n\033[0m" << std::endl;
        return;
    }

    std::vector<std::thread> threads;

    for (const std::string& isoDir : isoDirs) {
        // Construct a shell-escaped path
        std::string escapedIsoDir = shell_escape(isoDir);
        threads.emplace_back(cleanAndUnmountISO, escapedIsoDir);
        if (threads.size() >= 4) {
            for (std::thread& thread : threads) {
                thread.join();
            }
            threads.clear();
        }
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    std::cout << "\033[32mALL ISOS CLEANED\n\033[0m" << std::endl;
}


void listMountedISOs() {
    std::string path = "/mnt";
    int isoCount = 0;

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_directory() && entry.path().filename().string().find("iso") == 0) {
            // Print the directory number, name, and color with shell-escaped name
            std::string escapedDirectoryName = shell_escape(entry.path().filename().string());
            std::cout << "\033[1;35m" << ++isoCount << ". " << escapedDirectoryName << "\033[0m" << std::endl;
        }
    }

    if (isoCount == 0) {
        std::cout << "\033[31mNo ISO(s) mounted.\n\033[0m" << std::endl;
    }
}


// BIN/IMG CONVERSION FUNCTIONS


// Function to list and prompt the user to choose a file for conversion
std::string chooseFileToConvert(const std::vector<std::string>& files) {
    std::cout << "\033[32mFound the following .bin and .img files:\033[0m\n";
    for (size_t i = 0; i < files.size(); ++i) {
        std::cout << i + 1 << ": " << files[i] << "\n";
    }

    int choice;
    std::cout << "\033[32mEnter the number of the file you want to convert:\033[0m ";
    std::cin >> choice;

    if (choice >= 1 && choice <= static_cast<int>(files.size())) {
        return files[choice - 1];
    } else {
        std::cout << "\033[31mInvalid choice. Please choose a valid file.\033[31m\n";
        return "";
    }
}


std::vector<std::string> findBinImgFiles(const std::string& directory) {
    std::vector<std::string> fileNames;

    try {
        std::vector<std::future<void>> futures;
        std::mutex mutex; // Mutex for protecting the shared data
        const int maxThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4; // Maximum number of worker threads

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return std::tolower(c); }); // Convert extension to lowercase

                if ((ext == ".bin" || ext == ".img") && (entry.path().filename().string().find("data") == std::string::npos) && (entry.path().filename().string() != "terrain.bin") && (entry.path().filename().string() != "blocklist.bin")) {
                    if (std::filesystem::file_size(entry) >= 10'000'000) {
                        // Ensure the number of active threads doesn't exceed maxThreads
                        while (futures.size() >= maxThreads) {
                            // Wait for at least one thread to complete
                            auto it = std::find_if(futures.begin(), futures.end(),
                                [](const std::future<void>& f) { return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; });
                            if (it != futures.end()) {
                                it->get();
                                futures.erase(it);
                            }
                        }

                        // Create a task to process the file
                        futures.push_back(std::async(std::launch::async, [entry, &fileNames, &mutex] {
                            std::string fileName = entry.path().string();

                            // Lock the mutex before modifying the shared data
                            std::lock_guard<std::mutex> lock(mutex);

                            // Add the file name to the shared vector without shell escaping
                            fileNames.push_back(fileName);
                        }));
                    }
                }
            }
        }

        // Wait for the remaining tasks to complete
        for (auto& future : futures) {
            future.get();
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    return fileNames;
}


bool isCcd2IsoInstalled() {
    if (std::system("which ccd2iso > /dev/null 2>&1") == 0) {
        return true;
    } else {
        return false;
    }
}


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
    } else {
        // Execute the conversion using ccd2iso, with shell-escaped paths
        std::string conversionCommand = "ccd2iso " + shell_escape(inputPath) + " " + shell_escape(outputPath);
        int conversionStatus = std::system(conversionCommand.c_str());
        if (conversionStatus == 0) {
            std::cout << "\033[32mImage file converted to ISO: " << outputPath << std::endl;
        } else {
            std::cout << "\033[31mConversion of " << inputPath << " failed.\033[0m" << std::endl;
        }
    }
}

void convertBINsToISOs(const std::vector<std::string>& inputPaths, int numThreads) {
    if (!isCcd2IsoInstalled()) {
        std::cout << "\033[31mccd2iso is not installed. Please install it before using this option.\033[0m" << std::endl;
        return;
    }

    // Create a thread pool with a limited number of threads
    std::vector<std::thread> threads;
    int numCores = std::min(numThreads, static_cast<int>(std::thread::hardware_concurrency()));

    for (const std::string& inputPath : inputPaths) {
        if (inputPath == "") {
            break; // 
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



void processFilesInRange(int start, int end) {
        int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4; // Determine the number of threads based on CPU cores
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

void select_and_convert_files_to_iso() {
    std::string directoryPath = readInputLine("\033[32mEnter the directory path to search for .bin .img files:\033[0m ");
    
    if (directoryPath.empty()) {
        std::cout << "Path input is empty. Exiting." << std::endl;
        return;
    }

    binImgFiles = findBinImgFiles(directoryPath); // You need to define findBinImgFiles function.

    if (binImgFiles.empty()) {
        std::cout << "\033[33mNo .bin or .img files found in the specified directory and its subdirectories or all files are under 10MB.\033[0m" << std::endl;
    } else {
        for (int i = 0; i < binImgFiles.size(); i++) {
            std::cout << i + 1 << ". " << binImgFiles[i] << std::endl;
        }

        std::string input;

        while (true) {
            std::cout << "\033[31mChoose a file to process (enter the number or range e.g., 1-5 or 1 or simply press Enter to exit):\033[0m ";
            std::getline(std::cin, input);

            if (input.empty()) {
                std::cout << "Exiting..." << std::endl;
                break;
            }

            std::istringstream iss(input);
            int start, end;
            char dash;

            if (iss >> start) {
                if (iss >> dash && dash == '-' && iss >> end) {
                    // Range input (e.g., 1-5)
                    if (start >= 1 && start <= binImgFiles.size() && end >= start && end <= binImgFiles.size()) {
                        // Divide the work among threads (up to 4 cores)
                        std::vector<std::thread> threads;

                        for (int i = start; i <= end; i++) {
                            std::string selectedFile = binImgFiles[i - 1]; // No need to shell-escape

                            threads.emplace_back([selectedFile] {
                                convertBINToISO(selectedFile); // Removed numThreads argument
                            });
                        }

                        for (auto& thread : threads) {
                            thread.join();
                        }
                    } else {
                        std::cout << "\033[31mInvalid range. Please try again.\033[0m" << std::endl;
                    }
                } else if (start >= 1 && start <= binImgFiles.size()) {
                    // Single number input
                    int selectedIndex = start - 1;
                    std::string selectedFile = binImgFiles[selectedIndex]; // No need to shell-escape

                    convertBINToISO(selectedFile); // Removed numThreads argument
                } else {
                    std::cout << "\033[31mInvalid number. Please try again.\033[0m" << std::endl;
                }
            } else {
                std::cout << "\033[31mInvalid input format. Please try again.\033[0m" << std::endl;
            }
        }
    }
}



// MDF/MDS CONVERSION FUNCTIONS

std::vector<std::string> findMdsMdfFiles(const std::string& directory) {
    std::vector<std::string> fileNames;

    try {
        std::vector<std::future<void>> futures;
        std::mutex mutex; // Mutex for protecting the shared data
        const int maxThreads = 4; // Maximum number of worker threads

        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return std::tolower(c); }); // Convert extension to lowercase

                if (ext == ".mds" || ext == ".mdf") {
					if (ext == ".bin" || ext == ".img") {
                    if (std::filesystem::file_size(entry) >= 10'000'000) {
                    // Ensure the number of active threads doesn't exceed maxThreads
                    while (futures.size() >= maxThreads) {
                        // Wait for at least one thread to complete
                        auto it = std::find_if(futures.begin(), futures.end(),
                            [](const std::future<void>& f) { return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; });
                        if (it != futures.end()) {
                            it->get();
                            futures.erase(it);
                        }
                    }

                    // Create a task to process the file
                    futures.push_back(std::async(std::launch::async, [entry, &fileNames, &mutex] {
                        std::string fileName = entry.path().string();

                        // Lock the mutex before modifying the shared data
                        std::lock_guard<std::mutex> lock(mutex);

                        // Add the file name to the shared vector
                        fileNames.push_back(fileName);
                    }));
				  }
			   }  
            } 
        }
	}
        // Wait for the remaining tasks to complete
        for (auto& future : futures) {
            future.get();
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    return fileNames;
}

bool isMdf2IsoInstalled() {
    std::string command = "which " + shell_escape("mdf2iso");
    if (std::system((command + " > /dev/null 2>&1").c_str()) == 0) {
        return true;
    } else {
        return false;
    }
}

void convertMDFToISO(const std::string& inputPath) {
    // Check if the input file exists
    if (!std::ifstream(inputPath)) {
        std::cout << "\033[31mThe specified input file '" << inputPath << "' does not exist.\033[0m" << std::endl;
        return;
    }

    // Escape the inputPath before using it in shell commands
    std::string escapedInputPath = shell_escape(inputPath);

    // Define the output path for the ISO file with only the .iso extension
    std::string outputPath = inputPath.substr(0, inputPath.find_last_of(".")) + ".iso";

    // Escape the outputPath before using it in shell commands
    std::string escapedOutputPath = shell_escape(outputPath);

    // Check if the output ISO file already exists
    if (std::ifstream(outputPath)) {
        std::cout << "\033[33mThe output ISO file '" << outputPath << "' already exists. Skipping conversion.\033[0m" << std::endl;
    } else {
        // Execute the conversion using mdf2iso
        std::string conversionCommand = "mdf2iso " + escapedInputPath + " " + escapedOutputPath;
        int conversionStatus = std::system(conversionCommand.c_str());
        if (conversionStatus == 0) {
            std::cout << "\033[32mImage file converted to ISO: " << outputPath << std::endl;
        } else {
            std::cout << "\033[31mConversion of " << inputPath << " failed.\033[0m" << std::endl;
        }
    }
}

void convertMDFsToISOs(const std::vector<std::string>& inputPaths, int numThreads) {
    if (!isMdf2IsoInstalled()) {
        std::cout << "\033[31mmdf2iso is not installed. Please install it before using this option.\033[0m" << std::endl;
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

void processMDFFilesInRange(int start, int end) {
	int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4; // Determine the number of threads based on CPU cores
    std::vector<std::string> selectedFiles;
    for (int i = start; i <= end; i++) {
        // Escape the file path before using it in shell commands
        std::string escapedFilePath = shell_escape(mdfImgFiles[i - 1]);
        selectedFiles.push_back(escapedFilePath);
    }
    convertMDFsToISOs(selectedFiles, numThreads);
}


void select_and_convert_files_to_iso_mdf() {
        int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4; // Determine the number of threads based on CPU cores
    std::string directoryPath = readInputLine("\033[32mEnter the directory path to search for .mdf .mds files:\033[0m ");

    if (directoryPath.empty()) {
        std::cout << "Path input is empty. Exiting." << std::endl;
        return;
    }

    std::vector<std::string> mdfMdsFiles = findMdsMdfFiles(directoryPath);

    if (mdfMdsFiles.empty()) {
        std::cout << "No .mdf or .mds files found in the specified directory and its subdirectories or all files are under 10MB." << std::endl;
    } else {
        for (int i = 0; i < mdfMdsFiles.size(); i++) {
            std::cout << i + 1 << ". " << mdfMdsFiles[i] << std::endl;
        }

        std::string input;

        while (true) {
            std::cout << "Choose a file to process (enter the number or range e.g., 1-5 or 1 or simply press Enter to exit): ";
            std::getline(std::cin, input);

            if (input.empty()) {
                std::cout << "Exiting..." << std::endl;
                break;
            }

            std::istringstream iss(input);
            int start, end;
            char dash;

            if (iss >> start) {
                if (iss >> dash && dash == '-' && iss >> end) {
                    // Range input (e.g., 1-5)
                    if (start >= 1 && start <= mdfMdsFiles.size() && end >= start && end <= mdfMdsFiles.size()) {
                        // Divide the work among threads (up to 4 cores)
                        std::thread threads[4];
                        int range = (end - start + 1) / 4;

                        for (int i = 0; i < 4; i++) {
                            int threadStart = start + i * range;
                            int threadEnd = i == 3 ? end : threadStart + range - 1;
                            threads[i] = std::thread(processMdfMdsFilesInRange, mdfMdsFiles, threadStart, threadEnd);
                        }

                        for (int i = 0; i < 4; i++) {
                            threads[i].join();
                        }
                    } else {
                        std::cout << "\033[31mInvalid range. Please try again.\033[0m" << std::endl;
                    }
                } else if (start >= 1 && start <= mdfMdsFiles.size()) {
                    // Single number input
                    std::vector<std::string> selectedFiles;
                    selectedFiles.push_back(mdfMdsFiles[start - 1]);
                    convertMDFsToISOs(selectedFiles, numThreads); // Convert the selected file immediately
                } else {
                    std::cout << "\033[31mInvalid number. Please try again.\033[0m" << std::endl;
                }
            } else {
                std::cout << "Invalid input format. Please try again." << std::endl;
            }
        }
    }
}

void processMdfMdsFilesInRange(const std::vector<std::string>& mdfMdsFiles, int start, int end) {
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4; // Determine the number of threads based on CPU cores
    for (int i = start - 1; i < end; i++) {
        // Escape the file path before using it in shell commands
        std::string escapedFilePath = shell_escape(mdfMdsFiles[i]);

        std::vector<std::string> selectedFiles;
        selectedFiles.push_back(escapedFilePath);
        convertMDFsToISOs(selectedFiles, numThreads);
    }
}
