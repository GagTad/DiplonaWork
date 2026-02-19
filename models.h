#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace vlsi {
namespace core {

/**
 * @brief Represents a single cell/block in the chip layout
 *
 * Design Pattern: Value Object with some mutable state
 * SOLID: Single Responsibility - handles block geometry and placement
 */
class Block {
public:
    using ID = uint32_t;

    Block(ID id, const std::string& name, double width, double height, bool fixed = false);

    // Getters
    ID getId() const { return id_; }
    const std::string& getName() const { return name_; }
    double getWidth() const { return width_; }
    double getHeight() const { return height_; }
    double getX() const { return x_; }
    double getY() const { return y_; }
    bool isFixed() const { return isFixed_; }
    bool isFlipped() const { return isFlipped_; }

    // Computed properties
    double getCenterX() const { return x_ + (isFlipped_ ? height_ : width_) / 2.0; }
    double getCenterY() const { return y_ + (isFlipped_ ? width_ : height_) / 2.0; }
    double getRight() const { return x_ + (isFlipped_ ? height_ : width_); }
    double getBottom() const { return y_ + (isFlipped_ ? width_ : height_); }
    double getEffectiveWidth() const { return isFlipped_ ? height_ : width_; }
    double getEffectiveHeight() const { return isFlipped_ ? width_ : height_; }

    // Setters (only for non-fixed blocks during placement)
    void setPosition(double x, double y);
    void setFlipped(bool flipped);

    // Collision detection
    bool overlapsWith(const Block& other) const;
    double overlapArea(const Block& other) const;

private:
    ID id_;
    std::string name_;
    double width_;
    double height_;
    double x_;
    double y_;
    bool isFixed_;
    bool isFlipped_;  // 90-degree rotation
};

/**
 * @brief Represents a net connecting multiple blocks
 *
 * SOLID: Single Responsibility - manages connections and weights
 */
class Net {
public:
    using ID = uint32_t;

    Net(ID id, const std::string& name, double weight = 1.0);

    ID getId() const { return id_; }
    const std::string& getName() const { return name_; }
    double getWeight() const { return weight_; }
    const std::vector<Block::ID>& getBlockIds() const { return blockIds_; }

    void addBlock(Block::ID blockId);
    void setWeight(double weight) { weight_ = weight; }

    size_t size() const { return blockIds_.size(); }

private:
    ID id_;
    std::string name_;
    double weight_;
    std::vector<Block::ID> blockIds_;
};

/**
 * @brief Container for the entire chip design
 *
 * SOLID: Single Responsibility - manages the collection of blocks and nets
 * Design Pattern: Repository pattern for blocks and nets
 */
class Chip {
public:
    Chip(double width, double height);

    double getWidth() const { return width_; }
    double getHeight() const { return height_; }

    // Block management
    void addBlock(const Block& block);
    Block* getBlock(Block::ID id);
    const Block* getBlock(Block::ID id) const;
    Block* getBlockByName(const std::string& name);
    const std::vector<Block>& getBlocks() const { return blocks_; }
    std::vector<Block>& getBlocks() { return blocks_; }
    size_t getBlockCount() const { return blocks_.size(); }

    // Net management
    void addNet(const Net& net);
    const Net* getNet(Net::ID id) const;
    const std::vector<Net>& getNets() const { return nets_; }
    size_t getNetCount() const { return nets_.size(); }

    // Statistics
    double getTotalOverlapArea() const;
    bool hasOverlaps() const;

    // Validation
    bool isValidPlacement() const;

private:
    double width_;
    double height_;
    std::vector<Block> blocks_;
    std::vector<Net> nets_;
};

} // namespace core
} // namespace vlsi

#endif // MODELS_H
