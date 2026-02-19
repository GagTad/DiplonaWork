#include "Models.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vlsi {
namespace core {

// ============================================================================
// Block Implementation
// ============================================================================

Block::Block(ID id, const std::string& name, double width, double height, bool fixed)
    : id_(id), name_(name), width_(width), height_(height),
    x_(0.0), y_(0.0), isFixed_(fixed), isFlipped_(false) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Block dimensions must be positive");
    }
}

void Block::setPosition(double x, double y) {
    if (!isFixed_) {
        x_ = x;
        y_ = y;
    }
}

void Block::setFlipped(bool flipped) {
    if (!isFixed_) {
        isFlipped_ = flipped;
    }
}

bool Block::overlapsWith(const Block& other) const {
    // AABB (Axis-Aligned Bounding Box) collision detection
    double thisRight = getRight();
    double thisBottom = getBottom();
    double otherRight = other.getRight();
    double otherBottom = other.getBottom();

    // No overlap if one is completely to the left/right/above/below the other
    if (thisRight <= other.x_ || x_ >= otherRight ||
        thisBottom <= other.y_ || y_ >= otherBottom) {
        return false;
    }

    return true;
}

double Block::overlapArea(const Block& other) const {
    if (!overlapsWith(other)) {
        return 0.0;
    }

    // Calculate intersection rectangle
    double x1 = std::max(x_, other.x_);
    double y1 = std::max(y_, other.y_);
    double x2 = std::min(getRight(), other.getRight());
    double y2 = std::min(getBottom(), other.getBottom());

    double overlapWidth = x2 - x1;
    double overlapHeight = y2 - y1;

    return std::max(0.0, overlapWidth) * std::max(0.0, overlapHeight);
}

// ============================================================================
// Net Implementation
// ============================================================================

Net::Net(ID id, const std::string& name, double weight)
    : id_(id), name_(name), weight_(weight) {
    if (weight < 0) {
        throw std::invalid_argument("Net weight cannot be negative");
    }
}

void Net::addBlock(Block::ID blockId) {
    blockIds_.push_back(blockId);
}

// ============================================================================
// Chip Implementation
// ============================================================================

Chip::Chip(double width, double height)
    : width_(width), height_(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Chip dimensions must be positive");
    }
}

void Chip::addBlock(const Block& block) {
    blocks_.push_back(block);
}

Block* Chip::getBlock(Block::ID id) {
    if (id >= blocks_.size()) {
        return nullptr;
    }
    return &blocks_[id];
}

const Block* Chip::getBlock(Block::ID id) const {
    if (id >= blocks_.size()) {
        return nullptr;
    }
    return &blocks_[id];
}

Block* Chip::getBlockByName(const std::string& name) {
    for (auto& block : blocks_) {
        if (block.getName() == name) {
            return &block;
        }
    }
    return nullptr;
}

void Chip::addNet(const Net& net) {
    nets_.push_back(net);
}

const Net* Chip::getNet(Net::ID id) const {
    if (id >= nets_.size()) {
        return nullptr;
    }
    return &nets_[id];
}

double Chip::getTotalOverlapArea() const {
    double totalOverlap = 0.0;

    // Check all pairs of non-fixed blocks
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (blocks_[i].isFixed()) continue;

        for (size_t j = i + 1; j < blocks_.size(); ++j) {
            totalOverlap += blocks_[i].overlapArea(blocks_[j]);
        }
    }

    return totalOverlap;
}

bool Chip::hasOverlaps() const {
    return getTotalOverlapArea() > 1e-6; // Numerical tolerance
}

bool Chip::isValidPlacement() const {
    // Check all blocks are within chip boundaries
    for (const auto& block : blocks_) {
        if (block.getX() < 0 || block.getY() < 0 ||
            block.getRight() > width_ || block.getBottom() > height_) {
            return false;
        }
    }

    // Check no overlaps exist
    return !hasOverlaps();
}

} // namespace core
} // namespace vlsi
