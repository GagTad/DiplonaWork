#include "CostCalculator.h"
#include <algorithm>
#include <cmath>

namespace vlsi {
namespace solver {

double CostCalculator::calculateNetHPWL(const core::Net& net, const core::Chip& chip) {
    if (net.size() == 0) {
        return 0.0;
    }

    // Find bounding box of all connected blocks
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    for (auto blockId : net.getBlockIds()) {
        const auto* block = chip.getBlock(blockId);
        if (!block) continue;

        double centerX = block->getCenterX();
        double centerY = block->getCenterY();

        minX = std::min(minX, centerX);
        maxX = std::max(maxX, centerX);
        minY = std::min(minY, centerY);
        maxY = std::max(maxY, centerY);
    }

    // HPWL = half of the bounding box perimeter
    double hpwl = (maxX - minX) + (maxY - minY);
    return hpwl * net.getWeight();
}

double CostCalculator::calculateTotalHPWL(const core::Chip& chip) {
    double totalHPWL = 0.0;

    for (const auto& net : chip.getNets()) {
        totalHPWL += calculateNetHPWL(net, chip);
    }

    return totalHPWL;
}

double CostCalculator::calculateOverlapCost(const core::Chip& chip) {
    return chip.getTotalOverlapArea();
}

double CostCalculator::calculateTotalCost(const core::Chip& chip,
                                          double alpha,
                                          double beta) {
    double hpwl = calculateTotalHPWL(chip);
    double overlap = calculateOverlapCost(chip);
    double boundary = calculateBoundaryViolation(chip);

    // Total cost with adaptive weighting
    return alpha * hpwl + beta * (overlap + boundary);
}

double CostCalculator::calculateAdaptiveBeta(double temperature,
                                             double initialBeta,
                                             double finalBeta) {
    // Exponential increase as temperature decreases
    // When T = 1.0 (hot), beta ≈ initialBeta
    // When T → 0 (cold), beta → finalBeta

    // Normalize temperature to [0, 1]
    double t = std::max(0.0, std::min(1.0, temperature));

    // Exponential interpolation
    // beta(T) = initialBeta * (finalBeta/initialBeta)^(1-T)
    double ratio = finalBeta / initialBeta;
    double exponent = 1.0 - t;

    return initialBeta * std::pow(ratio, exponent);
}

bool CostCalculator::isWithinBounds(const core::Block& block, const core::Chip& chip) {
    return block.getX() >= 0 &&
           block.getY() >= 0 &&
           block.getRight() <= chip.getWidth() &&
           block.getBottom() <= chip.getHeight();
}

double CostCalculator::calculateBoundaryViolation(const core::Chip& chip) {
    double violation = 0.0;

    for (const auto& block : chip.getBlocks()) {
        if (block.isFixed()) continue;

        // Calculate how far outside the boundary the block extends
        if (block.getX() < 0) {
            violation += std::abs(block.getX()) * block.getEffectiveHeight();
        }
        if (block.getY() < 0) {
            violation += std::abs(block.getY()) * block.getEffectiveWidth();
        }
        if (block.getRight() > chip.getWidth()) {
            double excess = block.getRight() - chip.getWidth();
            violation += excess * block.getEffectiveHeight();
        }
        if (block.getBottom() > chip.getHeight()) {
            double excess = block.getBottom() - chip.getHeight();
            violation += excess * block.getEffectiveWidth();
        }
    }

    return violation;
}

} // namespace solver
} // namespace vlsi
