#include "annealerworker.h"
#include "costcalculator.h"
#include <QMutexLocker>
#include <cmath>
#include <algorithm>

namespace vlsi {
namespace solver {

AnnealerWorker::AnnealerWorker(QObject* parent)
    : QThread(parent),
      stopRequested_(false),
      currentCost_(std::numeric_limits<double>::max()),
      bestCost_(std::numeric_limits<double>::max()),
      rng_(std::random_device{}()),
      uniformDist_(0.0, 1.0),
      iterationsSinceImprovement_(0) {
}

AnnealerWorker::~AnnealerWorker() {
    requestStop();
    wait();
}

void AnnealerWorker::setChip(std::unique_ptr<core::Chip> chip) {
    QMutexLocker locker(&mutex_);
    chip_ = std::move(chip);
}

void AnnealerWorker::setParameters(const AnnealingParams& params) {
    QMutexLocker locker(&mutex_);
    params_ = params;
}

std::unique_ptr<core::Chip> AnnealerWorker::getResult() {
    QMutexLocker locker(&mutex_);
    if (bestChip_) {
        return std::make_unique<core::Chip>(*bestChip_);
    }
    return nullptr;
}

AnnealingStats AnnealerWorker::getStats() const {
    QMutexLocker locker(&mutex_);
    return stats_;
}

void AnnealerWorker::requestStop() {
    stopRequested_ = true;
}

void AnnealerWorker::run() {
    stopRequested_ = false;

    try {
        performAnnealing();
        emit finished(true);
    } catch (const std::exception& e) {
        emit finished(false);
    }
}

void AnnealerWorker::performAnnealing() {
    if (!chip_) {
        return;
    }

    // --- ՀԱՍՏԱՏՈՒՆ ԵՎ ՀԶՈՐ ՊԱՐԱՄԵՏՐԵՐ ---
    params_.initialTemp = 500000.0;
    params_.finalTemp = 0.1;
    params_.coolingRate = 0.98; // Շատ դանդաղ սառեցում
    params_.betaInitial = 1.00; // Սկզբում թույլ ենք տալիս overlap
    params_.betaFinal = 10000.0; // Վերջում խիստ արգելում ենք

    // ԿԱՐԵՎՈՐ ԼՈՒԾՈՒՄԸ. Քայլերի քանակը պետք է կախված լինի բլոկների քանակից
    int numMovableBlocks = 0;
    for (const auto& block : chip_->getBlocks()) {
        if (!block.isFixed()) {
            numMovableBlocks++;
        }
    }
    // Ամեն բլոկին տալիս ենք 20 հնարավորություն ամեն ջերմաստիճանում (տալիս է կայուն արդյունք)
    params_.movesPerTemp = numMovableBlocks * 20;

    // Initialize placement
    initializeRandomPlacement();

    // Evaluate initial cost
    double temperature = params_.initialTemp;
    currentCost_ = evaluateCost(temperature);
    bestCost_ = currentCost_;

    // Save best solution
    bestChip_ = std::make_unique<core::Chip>(*chip_);

    stats_.iteration = 0;
    stats_.currentTemp = temperature;
    stats_.currentCost = currentCost_;
    stats_.bestCost = bestCost_;

    emit progressUpdate(stats_);

    int totalMoves = 0;
    int acceptedMoves = 0;

    // Main annealing loop
    while (temperature > params_.finalTemp &&
           stats_.iteration < params_.maxIterations &&
           !stopRequested_) {

        int movesAtThisTemp = 0;
        int acceptedAtThisTemp = 0;

        // Վերահաշվարկում ենք currentCost-ը նոր ջերմաստիճանի և նոր Beta-ի պայմաններում
        currentCost_ = evaluateCost(temperature);

        // Perform multiple moves at this temperature
        for (int m = 0; m < params_.movesPerTemp && !stopRequested_; ++m) {

            bool moveGenerated = generateMove();
            if (!moveGenerated) {
                continue;
            }

            // Հաշվում ենք նոր արժեքը ԻՐԱԿԱՆ ջերմաստիճանով
            double newCost = evaluateCost(temperature);
            double deltaCost = newCost - currentCost_;

            // Accept or reject move
            if (acceptMove(deltaCost, temperature)) {
                currentCost_ = newCost;
                acceptedAtThisTemp++;
                acceptedMoves++;

                // Update best solution
                if (currentCost_ < bestCost_) {
                    bestCost_ = currentCost_;
                    bestChip_ = std::make_unique<core::Chip>(*chip_);
                }
            } else {
                // Reject move - undo it
                undoLastMove();
            }

            movesAtThisTemp++;
            totalMoves++;
        }

        // Update statistics
        stats_.iteration++;
        stats_.currentTemp = temperature;
        stats_.currentCost = currentCost_;
        stats_.bestCost = bestCost_;
        stats_.acceptedMoves = acceptedMoves;
        stats_.rejectedMoves = totalMoves - acceptedMoves;
        stats_.acceptanceRatio = movesAtThisTemp > 0 ?
                                     static_cast<double>(acceptedAtThisTemp) / movesAtThisTemp : 0.0;

        updateStats();

        if (stats_.iteration % 5 == 0) {
            emit progressUpdate(stats_);
        }

        // Cool down
        temperature *= params_.coolingRate;

        // Կանգ առնելու պայմանը (Սառած համակարգ)
        if (temperature < 1.0 && acceptedAtThisTemp == 0) {
            break;
        }
    }

    // Restore best solution
    if (bestChip_) {
        chip_ = std::make_unique<core::Chip>(*bestChip_);
    }

    // Final update
    updateStats();
    emit progressUpdate(stats_);
}

void AnnealerWorker::initializeRandomPlacement() {
    if (!chip_) return;

    double chipWidth = chip_->getWidth();
    double chipHeight = chip_->getHeight();

    // Սկզբնական դաշտը կենտրոնում (օրինակ՝ մեջտեղի 50%-ը)
    double marginX = chipWidth * 0.25;
    double marginY = chipHeight * 0.25;
    double usableWidth = chipWidth * 0.5;
    double usableHeight = chipHeight * 0.5;

    for (auto& block : chip_->getBlocks()) {
        if (block.isFixed()) {
            continue;
        }

        double x = marginX + uniformDist_(rng_) * (usableWidth - block.getEffectiveWidth());
        double y = marginY + uniformDist_(rng_) * (usableHeight - block.getEffectiveHeight());

        block.setPosition(x, y);
    }
}

bool AnnealerWorker::generateMove() {
    double r = uniformDist_(rng_);

    // Որքան սառչում է, այնքան Swap անելու հավանականությունը փոքրանում է
    double tempRatio = stats_.currentTemp / params_.initialTemp;

    if (r < 0.6) {
        return tryDisplacement();
    } else if (r < 0.6 + (0.3 * tempRatio) && params_.enableSwap) {
        return trySwap();
    } else if (params_.enableRotation) {
        return tryRotation();
    }

    return tryDisplacement();
}

bool AnnealerWorker::tryDisplacement() {
    if (!chip_ || chip_->getBlockCount() == 0) return false;

    std::vector<core::Block::ID> movableBlocks;
    for (const auto& block : chip_->getBlocks()) {
        if (!block.isFixed()) {
            movableBlocks.push_back(block.getId());
        }
    }

    if (movableBlocks.empty()) return false;

    std::uniform_int_distribution<size_t> blockDist(0, movableBlocks.size() - 1);
    core::Block::ID blockId = movableBlocks[blockDist(rng_)];

    auto* block = chip_->getBlock(blockId);
    if (!block) return false;

    // Record old state
    lastMove_.type = MoveType::Displacement;
    lastMove_.blockId1 = blockId;
    lastMove_.oldX1 = block->getX();
    lastMove_.oldY1 = block->getY();
    lastMove_.oldFlip1 = block->isFlipped();

    // Թռիչքի չափի հաշվարկ
    double tempRatio = stats_.currentTemp / params_.initialTemp;
    tempRatio = std::max(0.001, std::min(1.0, tempRatio));

    // Սովորական թռիչք (կախված ջերմաստիճանից)
    double calculatedJump = std::min(chip_->getWidth(), chip_->getHeight()) * 0.5 * tempRatio;

    // Մինիմալ թռիչք (որպեսզի կարողանա դուրս գալ այլ բլոկների տակից սառը վիճակում)
    double minJump = 10.0;

    double maxDisplacement = std::max(minJump, calculatedJump);
    double dx = (uniformDist_(rng_) - 0.5) * 2.0 * maxDisplacement;
    double dy = (uniformDist_(rng_) - 0.5) * 2.0 * maxDisplacement;

    double newX = block->getX() + dx;
    double newY = block->getY() + dy;

    // Clamp to boundaries
    newX = std::max(0.0, std::min(newX, chip_->getWidth() - block->getEffectiveWidth()));
    newY = std::max(0.0, std::min(newY, chip_->getHeight() - block->getEffectiveHeight()));

    block->setPosition(newX, newY);
    return true;
}

bool AnnealerWorker::trySwap() {
    if (!chip_ || chip_->getBlockCount() < 2) return false;

    std::vector<core::Block::ID> movableBlocks;
    for (const auto& block : chip_->getBlocks()) {
        if (!block.isFixed()) {
            movableBlocks.push_back(block.getId());
        }
    }

    if (movableBlocks.size() < 2) return false;

    std::uniform_int_distribution<size_t> blockDist(0, movableBlocks.size() - 1);
    core::Block::ID id1 = movableBlocks[blockDist(rng_)];
    core::Block::ID id2 = movableBlocks[blockDist(rng_)];

    if (id1 == id2) return false;

    auto* block1 = chip_->getBlock(id1);
    auto* block2 = chip_->getBlock(id2);

    if (!block1 || !block2) return false;

    lastMove_.type = MoveType::Swap;
    lastMove_.blockId1 = id1;
    lastMove_.blockId2 = id2;
    lastMove_.oldX1 = block1->getX();
    lastMove_.oldY1 = block1->getY();
    lastMove_.oldX2 = block2->getX();
    lastMove_.oldY2 = block2->getY();

    double tempX = block1->getX();
    double tempY = block1->getY();
    block1->setPosition(block2->getX(), block2->getY());
    block2->setPosition(tempX, tempY);

    return true;
}

bool AnnealerWorker::tryRotation() {
    if (!chip_ || chip_->getBlockCount() == 0) return false;

    std::vector<core::Block::ID> movableBlocks;
    for (const auto& block : chip_->getBlocks()) {
        if (!block.isFixed()) {
            movableBlocks.push_back(block.getId());
        }
    }

    if (movableBlocks.empty()) return false;

    std::uniform_int_distribution<size_t> blockDist(0, movableBlocks.size() - 1);
    core::Block::ID blockId = movableBlocks[blockDist(rng_)];

    auto* block = chip_->getBlock(blockId);
    if (!block) return false;

    lastMove_.type = MoveType::Rotation;
    lastMove_.blockId1 = blockId;
    lastMove_.oldFlip1 = block->isFlipped();
    lastMove_.oldX1 = block->getX();
    lastMove_.oldY1 = block->getY();

    block->setFlipped(!block->isFlipped());

    double newX = std::min(block->getX(), chip_->getWidth() - block->getEffectiveWidth());
    double newY = std::min(block->getY(), chip_->getHeight() - block->getEffectiveHeight());
    block->setPosition(std::max(0.0, newX), std::max(0.0, newY));

    return true;
}

void AnnealerWorker::undoLastMove() {
    if (!chip_) return;

    switch (lastMove_.type) {
        case MoveType::Displacement:
        case MoveType::Rotation: {
            auto* block = chip_->getBlock(lastMove_.blockId1);
            if (block) {
                block->setPosition(lastMove_.oldX1, lastMove_.oldY1);
                block->setFlipped(lastMove_.oldFlip1);
            }
            break;
        }
        case MoveType::Swap: {
            auto* block1 = chip_->getBlock(lastMove_.blockId1);
            auto* block2 = chip_->getBlock(lastMove_.blockId2);
            if (block1 && block2) {
                block1->setPosition(lastMove_.oldX1, lastMove_.oldY1);
                block2->setPosition(lastMove_.oldX2, lastMove_.oldY2);
            }
            break;
        }
    }
}

bool AnnealerWorker::acceptMove(double deltaCost, double temperature) {
    if (deltaCost < 0) {
        return true;
    }

    double probability = std::exp(-deltaCost / temperature);
    return uniformDist_(rng_) < probability;
}

double AnnealerWorker::evaluateCost(double temperature) {
    if (!chip_) return std::numeric_limits<double>::max();

    double normalizedTemp = (temperature - params_.finalTemp) /
                            (params_.initialTemp - params_.finalTemp);

    normalizedTemp = std::max(0.0, std::min(1.0, normalizedTemp));

    double beta = CostCalculator::calculateAdaptiveBeta(
        normalizedTemp, params_.betaInitial, params_.betaFinal);

    return CostCalculator::calculateTotalCost(*chip_, params_.alphaHPWL, beta);
}

void AnnealerWorker::updateStats() {
    if (!chip_) return;

    stats_.hpwl = CostCalculator::calculateTotalHPWL(*chip_);
    stats_.overlapArea = CostCalculator::calculateOverlapCost(*chip_);
}

} // namespace solver
} // namespace vlsi
