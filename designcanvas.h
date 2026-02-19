#ifndef DESIGN_CANVAS_H
#define DESIGN_CANVAS_H

#include "models.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <memory>
#include <map>

namespace vlsi {
namespace gui {

/**
 * @brief Custom block item with color coding
 */
class BlockItem : public QGraphicsRectItem {
public:
    BlockItem(const core::Block& block, QGraphicsItem* parent = nullptr);
    void updateFromBlock(const core::Block& block, bool hasOverlap);

private:
    core::Block::ID blockId_;
};

/**
 * @brief Interactive canvas for chip visualization
 *
 * SOLID: Single Responsibility - visualization only
 * Features: Zoom, Pan, Net display, Color coding
 */
class DesignCanvas : public QGraphicsView {
    Q_OBJECT

public:
    explicit DesignCanvas(QWidget* parent = nullptr);
    ~DesignCanvas() override;

    /**
     * @brief Set the chip to display
     */
    void setChip(const core::Chip* chip);

    /**
     * @brief Update display from current chip state
     */
    void refresh();

    /**
     * @brief Toggle net (ratsnest) visibility
     */
    void setShowNets(bool show);
    bool getShowNets() const { return showNets_; }

    /**
     * @brief Zoom controls
     */
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomReset();

    /**
     * @brief Get current zoom level
     */
    double getZoomLevel() const { return currentZoom_; }

signals:
    void blockSelected(core::Block::ID blockId);
    void zoomChanged(double level);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void createScene();
    void drawChipBoundary();
    void drawBlocks();
    void drawNets();
    void clearNets();
    void applyZoom(double factor);

    QGraphicsScene* scene_;
    const core::Chip* chip_;

    // Graphics items
    QGraphicsRectItem* chipBoundary_;
    std::map<core::Block::ID, BlockItem*> blockItems_;
    std::vector<QGraphicsLineItem*> netLines_;

    // View state
    bool showNets_;
    double currentZoom_;

    // Pan support
    bool isPanning_;
    QPoint lastPanPoint_;
};

} // namespace gui
} // namespace vlsi

#endif // DESIGN_CANVAS_H
