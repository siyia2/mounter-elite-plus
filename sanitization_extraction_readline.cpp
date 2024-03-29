#include "headers.h"


//	SANITISATION AND STRING STUFF

// Function to escape characters in a string for shell usage
std::string shell_escape(const std::string& s) {
    // Estimate the maximum size of the escaped string
    size_t max_size = s.size() * 4;  // Assuming every character might need escaping
    std::string escaped_string;
    escaped_string.reserve(max_size);

    // Iterate through each character in the input string
    for (char c : s) {
        // Check if the character needs escaping
        if (c == '\'') {
            // If the character is a single quote, replace it with the escaped version
            escaped_string.append("'\\''");
        } else {
            // Otherwise, keep the character unchanged
            escaped_string.push_back(c);
        }
    }

    // Return the escaped string enclosed in single quotes
    return "'" + escaped_string + "'";
}


// Function to extract directory and filename from a given path
std::pair<std::string, std::string> extractDirectoryAndFilename(const std::string& path) {
    // Initialize variables to store directory and filename
    std::string directory;
    std::string filename;

    // Initialize variables to track positions of slashes in the path
    std::size_t lastSlashPos = 0;
    std::size_t currentSlashPos = path.find_first_of("/\\");

    // Use stringstream for efficient string concatenation
    std::stringstream dirStream;

    // Loop through the path to extract directory components
    while (currentSlashPos != std::string::npos) {
        // Extract the current component between slashes
        std::string component = path.substr(lastSlashPos, currentSlashPos - lastSlashPos);

        // Limit each component to 10 characters or the first space gap
        std::size_t maxComponentSize = 28;
        std::size_t spacePos = component.find(' ');

        if (spacePos != std::string::npos && spacePos <= maxComponentSize) {
            component = component.substr(0, spacePos);
        } else {
            component = component.substr(0, maxComponentSize);
        }

        // Append the processed component to the stringstream
        dirStream << component << '/';
        // Update positions for the next iteration
        lastSlashPos = currentSlashPos + 1;
        currentSlashPos = path.find_first_of("/\\", lastSlashPos);
    }

    // Extract the last component as the filename
    filename = path.substr(lastSlashPos);

    // Remove the last '/' if the directory is not empty
    directory = dirStream.str();
    if (!directory.empty() && directory.back() == '/') {
        directory.pop_back();
    }

    // Replace specific Linux standard directories with custom strings
    std::unordered_map<std::string, std::string> replacements = {
        {"/home", "~"},
        {"/root", "/R"},
        // Add more replacements as needed
    };

    // Iterate through the replacements and update the directory
    for (const auto& [oldDir, newDir] : replacements) {
        size_t pos = directory.find(oldDir);
        if (pos != std::string::npos) {
            directory.replace(pos, oldDir.length(), newDir);
        }
    }

    // Return the pair of directory and filename
    return {directory, filename};
}


// Function for tab completion and history creation
std::string readInputLine(const std::string& prompt) {
    try {
        // Ensure the prompt is displayed before reading input
        std::cout << prompt;
        std::cout.flush();

        // Read a line of input using the readline function
        std::unique_ptr<char, decltype(&free)> input(readline(""), &free);

        // Check if input is not null (readline successful)
        if (input && input.get()[0] != '\0' && std::string(input.get()) != "\n") {
            // Add the input to the history
            add_history(input.get());

            // Convert the C-style string to a C++ string
            return std::string(input.get());
        }
    } catch (const std::exception& e) {
        // Log the error or handle it in a way suitable for your application
        std::cerr << "\033[91m" << e.what() << "\033[0m" << std::endl;
    }

    // Return an empty string if readline fails or input is empty
    return "";
}
