#include "DesignCanvas.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <cmath>

namespace vlsi {
namespace gui {

// ============================================================================
// BlockItem Implementation
// ============================================================================

BlockItem::BlockItem(const core::Block& block, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), blockId_(block.getId()) {
    updateFromBlock(block, false);
}

void BlockItem::updateFromBlock(const core::Block& block, bool hasOverlap) {
    // Update geometry
    setRect(block.getX(), block.getY(),
            block.getEffectiveWidth(), block.getEffectiveHeight());

    // Color coding based on state
    QColor fillColor;
    QPen outlinePen;

    if (block.isFixed()) {
        // Fixed blocks (pads/macros) - Grey
        fillColor = QColor(180, 180, 180, 200);
        outlinePen = QPen(QColor(100, 100, 100), 2);
    } else if (hasOverlap) {
        // Overlapping blocks - Red
        fillColor = QColor(255, 100, 100, 200);
        outlinePen = QPen(QColor(200, 0, 0), 2);
    } else {
        // Valid placement - Blue
        fillColor = QColor(100, 150, 255, 200);
        outlinePen = QPen(QColor(50, 100, 200), 2);
    }

    setBrush(QBrush(fillColor));
    setPen(outlinePen);

    // Enable hover and selection
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    // Tooltip with block info
    QString tooltip = QString("Block: %1\nSize: %2 x %3\nPos: (%4, %5)")
                          .arg(QString::fromStdString(block.getName()))
                          .arg(block.getWidth())
                          .arg(block.getHeight())
                          .arg(block.getX())
                          .arg(block.getY());

    if (block.isFlipped()) {
        tooltip += "\n[Rotated]";
    }

    setToolTip(tooltip);
}

// ============================================================================
// DesignCanvas Implementation
// ============================================================================

DesignCanvas::DesignCanvas(QWidget* parent)
    : QGraphicsView(parent),
    scene_(nullptr),
    chip_(nullptr),
    chipBoundary_(nullptr),
    showNets_(true),
    currentZoom_(1.0),
    isPanning_(false) {

    createScene();

    // Configure view
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::ScrollHandDrag);

    // Enable mouse tracking for panning
    setMouseTracking(true);
}

DesignCanvas::~DesignCanvas() {
    delete scene_;
}

void DesignCanvas::createScene() {
    scene_ = new QGraphicsScene(this);
    scene_->setBackgroundBrush(QBrush(QColor(250, 250, 250)));
    setScene(scene_);
}

void DesignCanvas::setChip(const core::Chip* chip) {
    chip_ = chip;
    refresh();
}

void DesignCanvas::refresh() {
    if (!chip_ || !scene_) {
        return;
    }

    // Clear existing items
    scene_->clear();
    blockItems_.clear();
    netLines_.clear();
    chipBoundary_ = nullptr;

    // Draw components
    drawChipBoundary();
    drawBlocks();

    if (showNets_) {
        drawNets();
    }

    // Fit view to chip
    zoomFit();
}

void DesignCanvas::drawChipBoundary() {
    if (!chip_) return;

    chipBoundary_ = scene_->addRect(
        0, 0, chip_->getWidth(), chip_->getHeight(),
        QPen(QColor(0, 0, 0), 3),
        QBrush(Qt::NoBrush)
        );

    chipBoundary_->setZValue(-1);
}

void DesignCanvas::drawBlocks() {
    if (!chip_) return;

    // First pass: determine which blocks have overlaps
    std::map<core::Block::ID, bool> hasOverlap;
    for (const auto& block : chip_->getBlocks()) {
        hasOverlap[block.getId()] = false;
    }

    for (size_t i = 0; i < chip_->getBlocks().size(); ++i) {
        const auto& block1 = chip_->getBlocks()[i];
        for (size_t j = i + 1; j < chip_->getBlocks().size(); ++j) {
            const auto& block2 = chip_->getBlocks()[j];
            if (block1.overlapsWith(block2)) {
                hasOverlap[block1.getId()] = true;
                hasOverlap[block2.getId()] = true;
            }
        }
    }

    // Second pass: create block items
    for (const auto& block : chip_->getBlocks()) {
        BlockItem* item = new BlockItem(block);
        item->updateFromBlock(block, hasOverlap[block.getId()]);
        scene_->addItem(item);
        blockItems_[block.getId()] = item;
    }
}

void DesignCanvas::drawNets() {
    if (!chip_) return;

    clearNets();

    // Draw ratsnest for each net
    for (const auto& net : chip_->getNets()) {
        if (net.size() < 2) continue;

        // Get centers of all blocks in the net
        std::vector<QPointF> centers;
        for (auto blockId : net.getBlockIds()) {
            const auto* block = chip_->getBlock(blockId);
            if (block) {
                centers.push_back(QPointF(block->getCenterX(), block->getCenterY()));
            }
        }

        // Draw lines from first block to all others (star topology)
        if (!centers.empty()) {
            QPointF center0 = centers[0];

            QPen netPen(QColor(100, 200, 100, 150), 1, Qt::DashLine);

            for (size_t i = 1; i < centers.size(); ++i) {
                QGraphicsLineItem* line = scene_->addLine(
                    center0.x(), center0.y(),
                    centers[i].x(), centers[i].y(),
                    netPen
                    );
                line->setZValue(-0.5);
                netLines_.push_back(line);
            }
        }
    }
}

void DesignCanvas::clearNets() {
    for (auto* line : netLines_) {
        scene_->removeItem(line);
        delete line;
    }
    netLines_.clear();
}

void DesignCanvas::setShowNets(bool show) {
    if (showNets_ == show) return;

    showNets_ = show;

    if (showNets_) {
        drawNets();
    } else {
        clearNets();
    }
}

void DesignCanvas::zoomIn() {
    applyZoom(1.2);
}

void DesignCanvas::zoomOut() {
    applyZoom(1.0 / 1.2);
}

void DesignCanvas::zoomFit() {
    if (!chip_) return;

    QRectF bounds(0, 0, chip_->getWidth(), chip_->getHeight());
    fitInView(bounds, Qt::KeepAspectRatio);
    currentZoom_ = transform().m11();
    emit zoomChanged(currentZoom_);
}

void DesignCanvas::zoomReset() {
    resetTransform();
    currentZoom_ = 1.0;
    emit zoomChanged(currentZoom_);
}

void DesignCanvas::applyZoom(double factor) {
    double newZoom = currentZoom_ * factor;

    // Limit zoom range
    if (newZoom < 0.1 || newZoom > 50.0) {
        return;
    }

    scale(factor, factor);
    currentZoom_ = newZoom;
    emit zoomChanged(currentZoom_);
}

void DesignCanvas::wheelEvent(QWheelEvent* event) {
    // Zoom with mouse wheel
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = event->angleDelta().y() > 0 ? 1.1 : 1.0 / 1.1;
        applyZoom(factor);
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DesignCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        isPanning_ = true;
        lastPanPoint_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void DesignCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (isPanning_) {
        QPoint delta = event->pos() - lastPanPoint_;
        lastPanPoint_ = event->pos();

        // Pan the view
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());

        event->accept();
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void DesignCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && isPanning_) {
        isPanning_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void DesignCanvas::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
}

} // namespace gui
} // namespace vlsi
