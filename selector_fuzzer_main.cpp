#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Forward declaration of fuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// Process a single file through the fuzzer
void process_file(const fs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open: " << path << "\n";
        return;
    }

    auto size = fs::file_size(path);
    std::vector<uint8_t> buf(size);
    ifs.read(reinterpret_cast<char*>(buf.data()), size);

    // Use actual bytes read - this prevents garbage if read fails
    auto bytes_read = ifs.gcount();
    if (bytes_read > 0) {
        std::cout << "Testing: " << path.filename() << "\n";
        LLVMFuzzerTestOneInput(buf.data(), bytes_read);
    }
}

// Process all files in a directory (recursively)
void process_directory(const fs::path& dir) {
    if (!fs::exists(dir)) {
        std::cerr << "Directory does not exist: " << dir << "\n";
        return;
    }

    std::cout << "Scanning directory: " << dir << "\n";

    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                process_file(entry.path());
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input-file-or-directory>...\n"
                  << "       Each argument can be either a file or directory.\n"
                  << "       Directories will be processed recursively.\n";
        return 1;
    }

    // Process each command line argument
    for (int i = 1; i < argc; ++i) {
        fs::path path(argv[i]);

        try {
            if (fs::is_directory(path)) {
                process_directory(path);
            } else if (fs::is_regular_file(path)) {
                process_file(path);
            } else {
                std::cerr << "Not a file or directory: " << path << "\n";
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error processing " << path << ": " << e.what() << "\n";
            continue;
        }
    }

    return 0;
}
