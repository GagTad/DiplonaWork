#include "mainwindow.h"
#include "benchmarkparser.h"
#include "costcalculator.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDateTime>
#include <QChart>
#include <QValueAxis>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>

//using namespace QtCharts;

namespace vlsi {
namespace gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      canvas_(nullptr),
      costChartView_(nullptr),
      costChart_(nullptr),
      costSeries_(nullptr),
      hpwlSeries_(nullptr),
      overlapSeries_(nullptr),
      isOptimizing_(false),
      elapsedSeconds_(0) {

    setupUi();

    // Default annealing parameters
    annealingParams_.initialTemp = 1000.0;
    annealingParams_.finalTemp = 0.1;
    annealingParams_.coolingRate = 0.95;
    annealingParams_.movesPerTemp = 100;
    annealingParams_.maxIterations = 10000;
    annealingParams_.alphaHPWL = 1.0;
    annealingParams_.betaInitial = 1.0;
    annealingParams_.betaFinal = 1000.0;

    // Setup update timer
    updateTimer_ = new QTimer(this);
    connect(updateTimer_, &QTimer::timeout, this, &MainWindow::onUpdateTimer);

    enableControls(false);
}

MainWindow::~MainWindow() {
    if (annealer_) {
        annealer_->requestStop();
        annealer_->wait();
    }
}

void MainWindow::setupUi() {
    setWindowTitle("VLSI Standard Cell Placement Tool");
    resize(1400, 900);

    createMenuBar();
    createToolBar();
    createCentralWidget();
    createDockWidgets();
    createStatusBar();
}

void MainWindow::createMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // File Menu
    QMenu* fileMenu = menuBar->addMenu("&File");

    loadAction_ = fileMenu->addAction("&Open Benchmark...");
    loadAction_->setShortcut(QKeySequence::Open);
    connect(loadAction_, &QAction::triggered, this, &MainWindow::onLoadBenchmark);

    saveAction_ = fileMenu->addAction("&Save Placement...");
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, &MainWindow::onSavePlacement);

    exportAction_ = fileMenu->addAction("&Export Image...");
    exportAction_->setShortcut(QKeySequence("Ctrl+E"));
    connect(exportAction_, &QAction::triggered, this, &MainWindow::onExportImage);

    fileMenu->addSeparator();

    exitAction_ = fileMenu->addAction("E&xit");
    exitAction_->setShortcut(QKeySequence::Quit);
    connect(exitAction_, &QAction::triggered, this, &MainWindow::onExit);

    // Placement Menu
    QMenu* placementMenu = menuBar->addMenu("&Placement");

    startAction_ = placementMenu->addAction("&Start Optimization");
    startAction_->setShortcut(QKeySequence("F5"));
    connect(startAction_, &QAction::triggered, this, &MainWindow::onStartPlacement);

    stopAction_ = placementMenu->addAction("S&top Optimization");
    stopAction_->setShortcut(QKeySequence("Shift+F5"));
    connect(stopAction_, &QAction::triggered, this, &MainWindow::onStopPlacement);

    resetAction_ = placementMenu->addAction("&Reset");
    connect(resetAction_, &QAction::triggered, this, &MainWindow::onResetPlacement);

    placementMenu->addSeparator();

    settingsAction_ = placementMenu->addAction("&Settings...");
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::onOpenSettings);

    // View Menu
    QMenu* viewMenu = menuBar->addMenu("&View");

    toggleNetsAction_ = viewMenu->addAction("Show &Nets");
    toggleNetsAction_->setCheckable(true);
    toggleNetsAction_->setChecked(true);
    connect(toggleNetsAction_, &QAction::triggered, this, &MainWindow::onToggleNets);

    viewMenu->addSeparator();

    zoomInAction_ = viewMenu->addAction("Zoom &In");
    zoomInAction_->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction_, &QAction::triggered, this, &MainWindow::onZoomIn);

    zoomOutAction_ = viewMenu->addAction("Zoom &Out");
    zoomOutAction_->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction_, &QAction::triggered, this, &MainWindow::onZoomOut);

    zoomFitAction_ = viewMenu->addAction("Zoom &Fit");
    zoomFitAction_->setShortcut(QKeySequence("Ctrl+0"));
    connect(zoomFitAction_, &QAction::triggered, this, &MainWindow::onZoomFit);
}

void MainWindow::createToolBar() {
    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);

    toolbar->addAction(loadAction_);
    toolbar->addAction(saveAction_);
    toolbar->addSeparator();
    toolbar->addAction(startAction_);
    toolbar->addAction(stopAction_);
    toolbar->addSeparator();
    toolbar->addAction(toggleNetsAction_);
    toolbar->addAction(zoomInAction_);
    toolbar->addAction(zoomOutAction_);
    toolbar->addAction(zoomFitAction_);
}

void MainWindow::createCentralWidget() {
    canvas_ = new DesignCanvas(this);
    setCentralWidget(canvas_);
}

void MainWindow::createDockWidgets() {
    // Statistics Dock
    QDockWidget* statsDock = new QDockWidget("Statistics", this);
    statsDock->setAllowedAreas(Qt::RightDockWidgetArea);

    QWidget* statsWidget = new QWidget(statsDock);
    QVBoxLayout* statsLayout = new QVBoxLayout(statsWidget);

    // Design Info Group
    QGroupBox* designGroup = new QGroupBox("Design Information");
    QFormLayout* designLayout = new QFormLayout(designGroup);

    blockCountLabel_ = new QLabel("0");
    netCountLabel_ = new QLabel("0");

    designLayout->addRow("Blocks:", blockCountLabel_);
    designLayout->addRow("Nets:", netCountLabel_);

    statsLayout->addWidget(designGroup);

    // Placement Quality Group
    QGroupBox* qualityGroup = new QGroupBox("Placement Quality");
    QFormLayout* qualityLayout = new QFormLayout(qualityGroup);

    hpwlLabel_ = new QLabel("N/A");
    overlapLabel_ = new QLabel("N/A");

    qualityLayout->addRow("HPWL:", hpwlLabel_);
    qualityLayout->addRow("Overlap:", overlapLabel_);

    statsLayout->addWidget(qualityGroup);

    // Annealing Status Group
    QGroupBox* annealGroup = new QGroupBox("Optimization Status");
    QFormLayout* annealLayout = new QFormLayout(annealGroup);

    iterationLabel_ = new QLabel("0");
    temperatureLabel_ = new QLabel("N/A");
    acceptanceRatioLabel_ = new QLabel("N/A");

    annealLayout->addRow("Iteration:", iterationLabel_);
    annealLayout->addRow("Temperature:", temperatureLabel_);
    annealLayout->addRow("Accept Ratio:", acceptanceRatioLabel_);

    statsLayout->addWidget(annealGroup);
    statsLayout->addStretch();

    statsDock->setWidget(statsWidget);
    addDockWidget(Qt::RightDockWidgetArea, statsDock);

    // Chart Dock
    QDockWidget* chartDock = new QDockWidget("Cost Evolution", this);
    chartDock->setAllowedAreas(Qt::BottomDockWidgetArea);

    // Create chart
    costChart_ = new QChart();
    costChart_->setTitle("Cost vs. Iteration");
    costChart_->setAnimationOptions(QChart::NoAnimation);

    costSeries_ = new QLineSeries();
    costSeries_->setName("Total Cost");
    costSeries_->setColor(QColor(0, 100, 200));

    hpwlSeries_ = new QLineSeries();
    hpwlSeries_->setName("HPWL");
    hpwlSeries_->setColor(QColor(100, 200, 0));

    overlapSeries_ = new QLineSeries();
    overlapSeries_->setName("Overlap");
    overlapSeries_->setColor(QColor(200, 0, 0));

    costChart_->addSeries(costSeries_);
    costChart_->addSeries(hpwlSeries_);
    costChart_->addSeries(overlapSeries_);

    costChart_->createDefaultAxes();

    costChartView_ = new QChartView(costChart_);
    costChartView_->setRenderHint(QPainter::Antialiasing);

    chartDock->setWidget(costChartView_);
    addDockWidget(Qt::BottomDockWidgetArea, chartDock);
}

void MainWindow::createStatusBar() {
    statusLabel_ = new QLabel("Ready");
    progressBar_ = new QProgressBar();
    progressBar_->setVisible(false);
    progressBar_->setMaximumWidth(200);

    statusBar()->addWidget(statusLabel_, 1);
    statusBar()->addPermanentWidget(progressBar_);
}

// ============================================================================
// Slot Implementations
// ============================================================================

void MainWindow::onLoadBenchmark() {
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Open Benchmark File",
        QString(),
        "Benchmark Files (*.bench *.txt);;All Files (*.*)"
    );

    if (filename.isEmpty()) {
        return;
    }

    try {
        chip_ = io::BenchmarkParser::parse(filename.toStdString());
        currentFilename_ = filename;

        canvas_->setChip(chip_.get());
        updateStatistics();

        enableControls(true);
        showInfo(QString("Loaded benchmark: %1 blocks, %2 nets")
                .arg(chip_->getBlockCount())
                .arg(chip_->getNetCount()));

    } catch (const io::ParseException& e) {
        showError(QString("Failed to parse benchmark: %1").arg(e.what()));
    } catch (const std::exception& e) {
        showError(QString("Error loading benchmark: %1").arg(e.what()));
    }
}

void MainWindow::onSavePlacement() {
    if (!chip_) {
        showError("No placement to save");
        return;
    }

    QString filename = QFileDialog::getSaveFileName(
        this,
        "Save Placement",
        QString(),
        "Benchmark Files (*.bench *.txt);;All Files (*.*)"
    );

    if (filename.isEmpty()) {
        return;
    }

    try {
        io::BenchmarkParser::exportToFile(*chip_, filename.toStdString());
        showInfo("Placement saved successfully");
    } catch (const std::exception& e) {
        showError(QString("Failed to save placement: %1").arg(e.what()));
    }
}

void MainWindow::onExportImage() {
    if (!chip_) {
        showError("No design to export");
        return;
    }

    QString filename = QFileDialog::getSaveFileName(
        this,
        "Export Image",
        QString(),
        "PNG Image (*.png);;JPEG Image (*.jpg);;All Files (*.*)"
    );

    if (filename.isEmpty()) {
        return;
    }

    QPixmap pixmap(canvas_->viewport()->size());
    canvas_->viewport()->render(&pixmap);

    if (pixmap.save(filename)) {
        showInfo("Image exported successfully");
    } else {
        showError("Failed to export image");
    }
}

void MainWindow::onExit() {
    if (isOptimizing_) {
        auto reply = QMessageBox::question(
            this,
            "Optimization Running",
            "Optimization is running. Are you sure you want to exit?",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::No) {
            return;
        }

        onStopPlacement();
    }

    close();
}

void MainWindow::onStartPlacement() {
    if (!chip_) {
        showError("No design loaded");
        return;
    }

    if (isOptimizing_) {
        showError("Optimization already running");
        return;
    }

    // Clear previous chart data
    clearChartData();

    // Create annealer worker
    annealer_ = std::make_unique<solver::AnnealerWorker>();

    // Make a copy of the chip for the annealer
    annealer_->setChip(std::make_unique<core::Chip>(*chip_));
    annealer_->setParameters(annealingParams_);

    // Connect signals
    connect(annealer_.get(), &solver::AnnealerWorker::progressUpdate,
            this, &MainWindow::onAnnealerProgress);
    connect(annealer_.get(), &solver::AnnealerWorker::finished,
            this, &MainWindow::onAnnealerFinished);
    connect(annealer_.get(), &solver::AnnealerWorker::betterSolutionFound,
            this, &MainWindow::onBetterSolutionFound);

    // Start optimization
    isOptimizing_ = true;
    elapsedSeconds_ = 0;
    updateTimer_->start(1000); // Update every second

    startAction_->setEnabled(false);
    stopAction_->setEnabled(true);
    progressBar_->setVisible(true);
    progressBar_->setRange(0, annealingParams_.maxIterations);

    statusLabel_->setText("Optimizing...");

    annealer_->start();
}

void MainWindow::onStopPlacement() {
    if (!isOptimizing_ || !annealer_) {
        return;
    }

    statusLabel_->setText("Stopping optimization...");
    annealer_->requestStop();
    annealer_->wait();

    updateTimer_->stop();
    isOptimizing_ = false;

    startAction_->setEnabled(true);
    stopAction_->setEnabled(false);

    statusLabel_->setText("Optimization stopped");
}

void MainWindow::onResetPlacement() {
    if (isOptimizing_) {
        showError("Cannot reset while optimizing");
        return;
    }

    if (!chip_) {
        return;
    }

    // Reload from file if available
    if (!currentFilename_.isEmpty()) {
        try {
            chip_ = io::BenchmarkParser::parse(currentFilename_.toStdString());
            canvas_->setChip(chip_.get());
            updateStatistics();
            clearChartData();
            showInfo("Placement reset");
        } catch (const std::exception& e) {
            showError(QString("Failed to reset: %1").arg(e.what()));
        }
    }
}

void MainWindow::onToggleNets() {
    if (canvas_) {
        canvas_->setShowNets(toggleNetsAction_->isChecked());
    }
}

void MainWindow::onZoomIn() {
    if (canvas_) {
        canvas_->zoomIn();
    }
}

void MainWindow::onZoomOut() {
    if (canvas_) {
        canvas_->zoomOut();
    }
}

void MainWindow::onZoomFit() {
    if (canvas_) {
        canvas_->zoomFit();
    }
}

void MainWindow::onAnnealerProgress(const solver::AnnealingStats& stats) {
    // Update progress bar
    progressBar_->setValue(stats.iteration);

    // Update labels
    iterationLabel_->setText(QString::number(stats.iteration));
    temperatureLabel_->setText(QString::number(stats.currentTemp, 'f', 2));
    acceptanceRatioLabel_->setText(QString::number(stats.acceptanceRatio * 100, 'f', 1) + "%");

    // Update chart
    updateChartData(stats.iteration, stats.currentCost);

    // Update canvas periodically
    if (stats.iteration % 50 == 0) {
        auto result = annealer_->getResult();
        if (result) {
            chip_ = std::move(result);
            canvas_->setChip(chip_.get());
            updateStatistics();
        }
    }
}

void MainWindow::onAnnealerFinished(bool success) {
    updateTimer_->stop();
    isOptimizing_ = false;

    startAction_->setEnabled(true);
    stopAction_->setEnabled(false);
    progressBar_->setVisible(false);

    if (success) {
        // Get final result
        auto result = annealer_->getResult();
        if (result) {
            chip_ = std::move(result);
            canvas_->setChip(chip_.get());
            updateStatistics();
        }

        QString message = QString("Optimization completed in %1 seconds")
            .arg(elapsedSeconds_);
        statusLabel_->setText(message);
        showInfo(message);
    } else {
        statusLabel_->setText("Optimization failed");
        showError("Optimization failed");
    }
}

void MainWindow::onBetterSolutionFound(double cost) {
    statusLabel_->setText(QString("Better solution found: Cost = %1").arg(cost, 0, 'f', 2));
}

void MainWindow::onOpenSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle("Annealing Settings");

    QFormLayout* layout = new QFormLayout(&dialog);

    QDoubleSpinBox* initialTempSpin = new QDoubleSpinBox();
    initialTempSpin->setRange(1.0, 10000.0);
    initialTempSpin->setValue(annealingParams_.initialTemp);
    layout->addRow("Initial Temperature:", initialTempSpin);

    QDoubleSpinBox* finalTempSpin = new QDoubleSpinBox();
    finalTempSpin->setRange(0.01, 100.0);
    finalTempSpin->setValue(annealingParams_.finalTemp);
    finalTempSpin->setDecimals(2);
    layout->addRow("Final Temperature:", finalTempSpin);

    QDoubleSpinBox* coolingRateSpin = new QDoubleSpinBox();
    coolingRateSpin->setRange(0.8, 0.99);
    coolingRateSpin->setValue(annealingParams_.coolingRate);
    coolingRateSpin->setDecimals(3);
    coolingRateSpin->setSingleStep(0.01);
    layout->addRow("Cooling Rate:", coolingRateSpin);

    QSpinBox* movesPerTempSpin = new QSpinBox();
    movesPerTempSpin->setRange(10, 1000);
    movesPerTempSpin->setValue(annealingParams_.movesPerTemp);
    layout->addRow("Moves Per Temperature:", movesPerTempSpin);

    QSpinBox* maxIterSpin = new QSpinBox();
    maxIterSpin->setRange(100, 100000);
    maxIterSpin->setValue(annealingParams_.maxIterations);
    layout->addRow("Max Iterations:", maxIterSpin);

    QDoubleSpinBox* betaInitialSpin = new QDoubleSpinBox();
    betaInitialSpin->setRange(0.1, 100.0);
    betaInitialSpin->setValue(annealingParams_.betaInitial);
    layout->addRow("Initial Beta:", betaInitialSpin);

    QDoubleSpinBox* betaFinalSpin = new QDoubleSpinBox();
    betaFinalSpin->setRange(1.0, 10000.0);
    betaFinalSpin->setValue(annealingParams_.betaFinal);
    layout->addRow("Final Beta:", betaFinalSpin);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        annealingParams_.initialTemp = initialTempSpin->value();
        annealingParams_.finalTemp = finalTempSpin->value();
        annealingParams_.coolingRate = coolingRateSpin->value();
        annealingParams_.movesPerTemp = movesPerTempSpin->value();
        annealingParams_.maxIterations = maxIterSpin->value();
        annealingParams_.betaInitial = betaInitialSpin->value();
        annealingParams_.betaFinal = betaFinalSpin->value();
    }
}

void MainWindow::onUpdateTimer() {
    elapsedSeconds_++;
}

// ============================================================================
// Helper Methods
// ============================================================================

void MainWindow::updateStatistics() {
    if (!chip_) {
        return;
    }

    blockCountLabel_->setText(QString::number(chip_->getBlockCount()));
    netCountLabel_->setText(QString::number(chip_->getNetCount()));

    double hpwl = solver::CostCalculator::calculateTotalHPWL(*chip_);
    double overlap = solver::CostCalculator::calculateOverlapCost(*chip_);

    hpwlLabel_->setText(QString::number(hpwl, 'f', 2));
    overlapLabel_->setText(QString::number(overlap, 'f', 2));
}

void MainWindow::updateChartData(double iteration, double cost) {
    if (!chip_) return;

    double hpwl = solver::CostCalculator::calculateTotalHPWL(*chip_);
    double overlap = solver::CostCalculator::calculateOverlapCost(*chip_);

    costSeries_->append(iteration, cost);
    hpwlSeries_->append(iteration, hpwl);
    overlapSeries_->append(iteration, overlap);

    // Limit number of points for performance
    if (costSeries_->count() > MAX_CHART_POINTS) {
        costSeries_->remove(0);
        hpwlSeries_->remove(0);
        overlapSeries_->remove(0);
    }

    // Update axes
    if (costSeries_->count() > 1) {
        costChart_->createDefaultAxes();
    }
}

void MainWindow::clearChartData() {
    costSeries_->clear();
    hpwlSeries_->clear();
    overlapSeries_->clear();
}

void MainWindow::enableControls(bool enabled) {
    startAction_->setEnabled(enabled);
    saveAction_->setEnabled(enabled);
    exportAction_->setEnabled(enabled);
    resetAction_->setEnabled(enabled);
}

void MainWindow::showError(const QString& message) {
    QMessageBox::critical(this, "Error", message);
}

void MainWindow::showInfo(const QString& message) {
    QMessageBox::information(this, "Information", message);
}

} // namespace gui
} // namespace vlsi
