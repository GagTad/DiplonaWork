#ifndef BENCHMARK_PARSER_H
#define BENCHMARK_PARSER_H

#include "models.h"
#include <string>
#include <memory>
#include <stdexcept>

namespace vlsi {
namespace io {

/**
 * @brief Exception thrown during parsing errors
 */
class ParseException : public std::runtime_error {
public:
    explicit ParseException(const std::string& message, int line = -1);
    int getLineNumber() const { return lineNumber_; }

private:
    int lineNumber_;
};

/**
 * @brief Parser for VLSI benchmark files
 *
 * SOLID: Single Responsibility - file parsing only
 * SOLID: Open/Closed - extensible for different file formats
 *
 * File Format:
 * SIZE <width> <height>
 * BLOCK <name> <width> <height> [FIXED <x> <y>]
 * NET <name> <block1> <block2> ... [WEIGHT <weight>]
 */
class BenchmarkParser {
public:
    /**
     * @brief Parse a benchmark file and create a Chip object
     *
     * @param filename Path to the benchmark file
     * @return Unique pointer to the parsed Chip
     * @throws ParseException on parsing errors
     */
    static std::unique_ptr<core::Chip> parse(const std::string& filename);

    /**
     * @brief Validate benchmark file syntax without full parsing
     *
     * @param filename Path to the benchmark file
     * @return true if file is valid, false otherwise
     */
    static bool validate(const std::string& filename);

    /**
     * @brief Export chip to benchmark format
     *
     * @param chip The chip to export
     * @param filename Output filename
     */
    static void exportToFile(const core::Chip& chip, const std::string& filename);

private:
    BenchmarkParser() = delete; // Static class

    // Helper parsing functions
    static void parseSize(const std::string& line, double& width, double& height);
    static void parseBlock(const std::string& line,
                           std::string& name,
                           double& width,
                           double& height,
                           bool& isFixed,
                           double& x,
                           double& y);
    static void parseNet(const std::string& line,
                         std::string& name,
                         std::vector<std::string>& blockNames,
                         double& weight);

    // String utilities
    static std::vector<std::string> tokenize(const std::string& line);
    static std::string trim(const std::string& str);
    static bool startsWith(const std::string& str, const std::string& prefix);
};

} // namespace io
} // namespace vlsi

#endif // BENCHMARK_PARSER_H
