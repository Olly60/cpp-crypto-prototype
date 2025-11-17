#include <filesystem>
#include <fstream>
#include "types.h"

namespace fs = std::filesystem;

namespace v1 {

    fs::path findOrMakeFile(const fs::path& baseDir, uintmax_t limitBytes, std::string &fileName) {
        // Ensure the directory exists
        fs::create_directories(baseDir);
        for (uint64_t i = 0;; i++) {
            // Build file path: baseDir/block<i>.dat
            std::ostringstream name;
            name << fileName << std::to_string(i) << ".dat";
            fs::path p = baseDir / name.str();
            // If file doesn't exist → this is the new one
            if (!fs::exists(p)) return p;
            // File exists → check size
            std::error_code ec;
            uintmax_t size = fs::file_size(p, ec);
            if (ec) continue; // Could not read file; treat as unusable and continue
            // If size is within the limit, reuse it
            if (size <= limitBytes) return p;
            // Otherwise size > limit → continue to next number
        }
    }

	static void addUXTO()
	{
		fs::create_directories("chain/blocks");
	}
	static void addBlock() {
		fs::create_directories("chain/blocks");
		fs:create_file
		bool findValidBlockFile();
		while (smallEnough) {
			std::error_code ec;
			uintmax_t size = fs::file_size(p, ec);
			if (ec) {
				smallEnough = false; // file missing or unreadable
			}

			const uintmax_t limit = 128000000; // 128 million bytes

			smallEnough = size <= limit;
		}
	}
}