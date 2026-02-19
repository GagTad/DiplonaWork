#ifndef ANNEALER_WORKER_H
#define ANNEALER_WORKER_H

#include "models.h"
#include <QThread>
#include <QMutex>
#include <memory>
#include <random>
#include <atomic>

namespace vlsi {
namespace solver {

/**
 * @brief Configuration parameters for Simulated Annealing
 */
struct AnnealingParams {
    double initialTemp = 1000.0;
    double finalTemp = 0.1;
    double coolingRate = 0.95;
    int movesPerTemp = 100;
    int maxIterations = 10000;

    double alphaHPWL = 1.0;
    double betaInitial = 1.0;
    double betaFinal = 1000.0;

    bool enableRotation = true;
    bool enableSwap = true;

    // Convergence criteria
    double costTolerance = 1e-6;
    int stagnationLimit = 50; // iterations without improvement
};

/**
 * @brief Statistics for monitoring annealing progress
 */
struct AnnealingStats {
    int iteration = 0;
    double currentTemp = 0.0;
    double currentCost = 0.0;
    double bestCost = 0.0;
    double hpwl = 0.0;
    double overlapArea = 0.0;
    int acceptedMoves = 0;
    int rejectedMoves = 0;
    double acceptanceRatio = 0.0;
};

/**
 * @brief Adaptive Simulated Annealing solver running in a separate thread
 *
 * SOLID: Single Responsibility - placement optimization only
 * SOLID: Dependency Inversion - operates on abstract Chip interface
 * Design Pattern: Worker Thread pattern with Qt Signals/Slots
 */
class AnnealerWorker : public QThread {
    Q_OBJECT

public:
    explicit AnnealerWorker(QObject* parent = nullptr);
    ~AnnealerWorker() override;

    /**
     * @brief Set the chip to optimize (makes a copy)
     */
    void setChip(std::unique_ptr<core::Chip> chip);

    /**
     * @brief Set annealing parameters
     */
    void setParameters(const AnnealingParams& params);

    /**
     * @brief Get the optimized chip (thread-safe)
     */
    std::unique_ptr<core::Chip> getResult();

    /**
     * @brief Get current statistics (thread-safe)
     */
    AnnealingStats getStats() const;

    /**
     * @brief Request cancellation of the optimization
     */
    void requestStop();

signals:
    /**
     * @brief Emitted periodically during optimization
     */
    void progressUpdate(const AnnealingStats& stats);

    /**
     * @brief Emitted when optimization completes
     */
    void finished(bool success);

    /**
     * @brief Emitted when a better solution is found
     */
    void betterSolutionFound(double cost);

protected:
    void run() override;

private:
    // Algorithm implementation
    void performAnnealing();
    void initializeRandomPlacement();
    bool generateMove();
    void undoLastMove();
    bool acceptMove(double deltaCost, double temperature);

    // Move types
    enum class MoveType {
        Displacement,
        Swap,
        Rotation
    };

    bool tryDisplacement();
    bool trySwap();
    bool tryRotation();

    // Cost evaluation
    double evaluateCost(double temperature);
    void updateStats();

    // Data members
    std::unique_ptr<core::Chip> chip_;
    std::unique_ptr<core::Chip> bestChip_;
    AnnealingParams params_;

    mutable QMutex mutex_;
    std::atomic<bool> stopRequested_;

    AnnealingStats stats_;
    double currentCost_;
    double bestCost_;

    // Random number generation
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniformDist_;

    // Move tracking for undo
    struct MoveRecord {
        MoveType type;
        core::Block::ID blockId1;
        core::Block::ID blockId2;
        double oldX1, oldY1;
        double oldX2, oldY2;
        bool oldFlip1, oldFlip2;
    };
    MoveRecord lastMove_;

    // Convergence tracking
    int iterationsSinceImprovement_;
};

} // namespace solver
} // namespace vlsi

#endif // ANNEALER_WORKER_H
