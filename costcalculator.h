#ifndef COST_CALCULATOR_H
#define COST_CALCULATOR_H

#include "models.h"
#include <limits>

namespace vlsi {
namespace solver {

/**
 * @brief Static utility class for cost function calculations
 *
 * SOLID: Single Responsibility - cost computation only
 * Design Pattern: Strategy pattern for different cost metrics
 */
class CostCalculator {
public:
    /**
     * @brief Calculate Half-Perimeter Wire Length (HPWL) for a net
     *
     * HPWL is the industry-standard metric for estimating wire length.
     * For a net connecting blocks, it's the half-perimeter of the bounding box
     * enclosing all connected block centers.
     */
    static double calculateNetHPWL(const core::Net& net, const core::Chip& chip);

    /**
     * @brief Calculate total weighted HPWL for all nets
     */
    static double calculateTotalHPWL(const core::Chip& chip);

    /**
     * @brief Calculate total overlap area between all blocks
     *
     * This is a constraint violation metric that must be driven to zero
     */
    static double calculateOverlapCost(const core::Chip& chip);

    /**
     * @brief Combined cost function with adaptive overlap penalty
     *
     * Cost = alpha * HPWL + beta(T) * Overlap
     *
     * @param chip Current chip state
     * @param alpha Wire length weight (typically 1.0)
     * @param beta Overlap penalty weight (increases as T decreases)
     */
    static double calculateTotalCost(const core::Chip& chip, double alpha, double beta);

    /**
     * @brief Calculate adaptive beta based on temperature
     *
     * Beta increases as temperature decreases to force overlap resolution
     * in the final stages of annealing.
     *
     * @param temperature Current temperature (0 to 1, normalized)
     * @param initialBeta Starting beta value
     * @param finalBeta Ending beta value
     */
    static double calculateAdaptiveBeta(double temperature,
                                        double initialBeta = 1.0,
                                        double finalBeta = 1000.0);

    /**
     * @brief Check if placement is within chip boundaries
     */
    static bool isWithinBounds(const core::Block& block, const core::Chip& chip);

    /**
     * @brief Calculate out-of-bounds penalty
     */
    static double calculateBoundaryViolation(const core::Chip& chip);

private:
    CostCalculator() = delete; // Pure static class
};

} // namespace solver
} // namespace vlsi

#endif // COST_CALCULATOR_H
