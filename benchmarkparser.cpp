#include "benchmarkparser.h".h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <map>

namespace vlsi {
namespace io {

// ============================================================================
// ParseException Implementation
// ============================================================================

ParseException::ParseException(const std::string& message, int line)
    : std::runtime_error(line >= 0 ?
                             "Parse error at line " + std::to_string(line) + ": " + message :
                             "Parse error: " + message),
    lineNumber_(line) {
}

// ============================================================================
// BenchmarkParser Implementation
// ============================================================================

std::unique_ptr<core::Chip> BenchmarkParser::parse(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw ParseException("Cannot open file: " + filename);
    }

    std::unique_ptr<core::Chip> chip;
    std::map<std::string, core::Block::ID> blockNameToId;
    std::string line;
    int lineNumber = 0;
    bool sizeFound = false;

    try {
        while (std::getline(file, line)) {
            lineNumber++;
            line = trim(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Parse SIZE directive
            if (startsWith(line, "SIZE")) {
                if (sizeFound) {
                    throw ParseException("Duplicate SIZE directive", lineNumber);
                }

                double width, height;
                parseSize(line, width, height);
                chip = std::make_unique<core::Chip>(width, height);
                sizeFound = true;
            }
            // Parse BLOCK directive
            else if (startsWith(line, "BLOCK")) {
                if (!chip) {
                    throw ParseException("BLOCK found before SIZE directive", lineNumber);
                }

                std::string name;
                double width, height, x, y;
                bool isFixed;

                parseBlock(line, name, width, height, isFixed, x, y);

                // Check for duplicate block names
                if (blockNameToId.find(name) != blockNameToId.end()) {
                    throw ParseException("Duplicate block name: " + name, lineNumber);
                }

                core::Block::ID id = chip->getBlockCount();
                core::Block block(id, name, width, height, isFixed);

                if (isFixed) {
                    block.setPosition(x, y);
                }

                chip->addBlock(block);
                blockNameToId[name] = id;
            }
            // Parse NET directive
            else if (startsWith(line, "NET")) {
                if (!chip) {
                    throw ParseException("NET found before SIZE directive", lineNumber);
                }

                std::string name;
                std::vector<std::string> blockNames;
                double weight;

                parseNet(line, name, blockNames, weight);

                // Validate that all blocks exist
                for (const auto& blockName : blockNames) {
                    if (blockNameToId.find(blockName) == blockNameToId.end()) {
                        throw ParseException("Unknown block in net: " + blockName, lineNumber);
                    }
                }

                core::Net::ID netId = chip->getNetCount();
                core::Net net(netId, name, weight);

                for (const auto& blockName : blockNames) {
                    net.addBlock(blockNameToId[blockName]);
                }

                chip->addNet(net);
            }
            else {
                throw ParseException("Unknown directive: " + line, lineNumber);
            }
        }

        if (!sizeFound) {
            throw ParseException("No SIZE directive found in file");
        }

    } catch (const std::exception& e) {
        throw ParseException(std::string("Error parsing file: ") + e.what(), lineNumber);
    }

    return chip;
}

bool BenchmarkParser::validate(const std::string& filename) {
    try {
        parse(filename);
        return true;
    } catch (const ParseException&) {
        return false;
    }
}

void BenchmarkParser::exportToFile(const core::Chip& chip, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw ParseException("Cannot create output file: " + filename);
    }

    // Write SIZE
    file << "SIZE " << chip.getWidth() << " " << chip.getHeight() << "\n\n";

    // Write BLOCKs
    file << "# Blocks\n";
    for (const auto& block : chip.getBlocks()) {
        file << "BLOCK " << block.getName()
        << " " << block.getWidth()
        << " " << block.getHeight();

        if (block.isFixed()) {
            file << " FIXED " << block.getX() << " " << block.getY();
        }

        file << "\n";
    }

    file << "\n# Nets\n";
    // Write NETs
    for (const auto& net : chip.getNets()) {
        file << "NET " << net.getName();

        for (auto blockId : net.getBlockIds()) {
            const auto* block = chip.getBlock(blockId);
            if (block) {
                file << " " << block->getName();
            }
        }

        if (net.getWeight() != 1.0) {
            file << " WEIGHT " << net.getWeight();
        }

        file << "\n";
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

void BenchmarkParser::parseSize(const std::string& line, double& width, double& height) {
    auto tokens = tokenize(line);
    if (tokens.size() != 3) {
        throw ParseException("SIZE requires exactly 2 arguments: SIZE <width> <height>");
    }

    try {
        width = std::stod(tokens[1]);
        height = std::stod(tokens[2]);
    } catch (const std::exception&) {
        throw ParseException("Invalid SIZE dimensions");
    }

    if (width <= 0 || height <= 0) {
        throw ParseException("SIZE dimensions must be positive");
    }
}

void BenchmarkParser::parseBlock(const std::string& line,
                                 std::string& name,
                                 double& width,
                                 double& height,
                                 bool& isFixed,
                                 double& x,
                                 double& y) {
    auto tokens = tokenize(line);
    if (tokens.size() < 4) {
        throw ParseException("BLOCK requires at least 3 arguments: BLOCK <name> <width> <height>");
    }

    name = tokens[1];

    try {
        width = std::stod(tokens[2]);
        height = std::stod(tokens[3]);
    } catch (const std::exception&) {
        throw ParseException("Invalid BLOCK dimensions");
    }

    if (width <= 0 || height <= 0) {
        throw ParseException("BLOCK dimensions must be positive");
    }

    isFixed = false;
    x = y = 0.0;

    // Check for FIXED keyword
    if (tokens.size() >= 7 && tokens[4] == "FIXED") {
        isFixed = true;
        try {
            x = std::stod(tokens[5]);
            y = std::stod(tokens[6]);
        } catch (const std::exception&) {
            throw ParseException("Invalid FIXED coordinates");
        }
    }
}

void BenchmarkParser::parseNet(const std::string& line,
                               std::string& name,
                               std::vector<std::string>& blockNames,
                               double& weight) {
    auto tokens = tokenize(line);
    if (tokens.size() < 2) {
        throw ParseException("NET requires at least 1 argument: NET <name> [blocks...]");
    }

    name = tokens[1];
    blockNames.clear();
    weight = 1.0;

    size_t i = 2;
    while (i < tokens.size()) {
        if (tokens[i] == "WEIGHT") {
            if (i + 1 >= tokens.size()) {
                throw ParseException("WEIGHT keyword requires a value");
            }
            try {
                weight = std::stod(tokens[i + 1]);
            } catch (const std::exception&) {
                throw ParseException("Invalid WEIGHT value");
            }
            break;
        }
        blockNames.push_back(tokens[i]);
        i++;
    }
}

std::vector<std::string> BenchmarkParser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

std::string BenchmarkParser::trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }

    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

bool BenchmarkParser::startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           str.compare(0, prefix.size(), prefix) == 0;
}

} // namespace io
} // namespace vlsi
