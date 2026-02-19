#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "models.h"
#include "annealerworker.h"
#include "designcanvas.h"
#include <QMainWindow>
#include <QChartView>
#include <QtCharts> QT_CHARTS_USE_NAMESPACE
#include <QLineSeries>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

namespace vlsi {
namespace gui {

/**
 * @brief Main application window
 *
 * SOLID: Single Responsibility - UI orchestration
 * Design Pattern: MVC - MainWindow is the Controller
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // File operations
    void onLoadBenchmark();
    void onSavePlacement();
    void onExportImage();
    void onExit();

    // Placement operations
    void onStartPlacement();
    void onStopPlacement();
    void onResetPlacement();

    // View operations
    void onToggleNets();
    void onZoomIn();
    void onZoomOut();
    void onZoomFit();

    // Annealer callbacks
    void onAnnealerProgress(const solver::AnnealingStats& stats);
    void onAnnealerFinished(bool success);
    void onBetterSolutionFound(double cost);

    // Settings
    void onOpenSettings();

    // Timer
    void onUpdateTimer();

private:
    void setupUi();
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void createDockWidgets();
    void createCentralWidget();

    void updateStatistics();
    void updateChartData(double iteration, double cost);
    void clearChartData();

    void enableControls(bool enabled);
    void showError(const QString& message);
    void showInfo(const QString& message);

    // UI Components
    DesignCanvas* canvas_;

    // Charts (using QtCharts)
    QChartView* costChartView_;
    QChart* costChart_;
    QLineSeries* costSeries_;
    QLineSeries* hpwlSeries_;
    QLineSeries* overlapSeries_;

    // Status widgets
    QLabel* statusLabel_;
    QProgressBar* progressBar_;
    QLabel* hpwlLabel_;
    QLabel* overlapLabel_;
    QLabel* blockCountLabel_;
    QLabel* netCountLabel_;
    QLabel* iterationLabel_;
    QLabel* temperatureLabel_;
    QLabel* acceptanceRatioLabel_;

    // Actions
    QAction* loadAction_;
    QAction* saveAction_;
    QAction* exportAction_;
    QAction* exitAction_;
    QAction* startAction_;
    QAction* stopAction_;
    QAction* resetAction_;
    QAction* toggleNetsAction_;
    QAction* zoomInAction_;
    QAction* zoomOutAction_;
    QAction* zoomFitAction_;
    QAction* settingsAction_;

    // Data
    std::unique_ptr<core::Chip> chip_;
    std::unique_ptr<solver::AnnealerWorker> annealer_;
    solver::AnnealingParams annealingParams_;

    // State
    QString currentFilename_;
    bool isOptimizing_;
    QTimer* updateTimer_;
    int elapsedSeconds_;

    // Chart data limits
    static constexpr int MAX_CHART_POINTS = 1000;
};

} // namespace gui
} // namespace vlsi

#endif // MAIN_WINDOW_H
