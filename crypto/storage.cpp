#include <filesystem>
#include <fstream>
#include "types.h"
#include "utils.h"

namespace fs = std::filesystem;


static fs::path findOrMakeFile(const fs::path& baseDir, uintmax_t limitBytes) {
        // Ensure the directory exists
        fs::create_directories(baseDir);
        for (uint64_t i = 0;; i++) {
            // Build file path: baseDir/block<i>.dat
            std::ostringstream name;
            name << i << ".dat";
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

namespace v1 {
	static void addUXTO(UTXO &utxo)
	{
		fs::create_directories("chain/blocks");
	}
    static void addBlock(const Block& block) {
        std::fstream file(findOrMakeFile("chain/blocks", 128000000), std::fstream::binary | std::fstream::app | std::fstream::in);

        std::vector<uint8_t> buf;

        uint64_t blockAmount{};
        buf.resize(sizeof(uint64_t));
        file.read(reinterpret_cast<char*>(buf.data()), sizeof(uint64_t));
        blockAmount = formatNumber<uint64_t>(buf.data());

        for (uint64_t i = 0; i < blockAmount; i++) {

        }

		
	}
}