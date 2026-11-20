#pragma once

#include "racaldana.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QMainWindow>
#include <QPointF>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>
#include <QTranslator>

#include <array>
#include <deque>
#include <initializer_list>
#include <utility>

class QAction;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QMenu;
class QTabWidget;
class QwtPlot;
class QwtPlotCurve;
class QwtPlotGrid;

class MainWindow final : public QMainWindow
{
    Q_DECLARE_TR_FUNCTIONS(MainWindow)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    using Choice = std::pair<const char *, const char *>;

    struct InputControls {
        QComboBox *coupling;
        QComboBox *impedance;
        QComboBox *slope;
        QComboBox *attenuation;
        QComboBox *trigger;
        QDoubleSpinBox *level;
    };

    void buildInterface();
    void createMenus();
    void installLanguage(const QString &code);
    void changeLanguage(const QString &code);
    QWidget *createMeasurementTab();
    QWidget *createInputTab();
    QWidget *createAdvancedTab();
    QComboBox *addChoice(QFormLayout *form, const char *label, const QString &key,
                         std::initializer_list<Choice> choices, int defaultIndex = 0);
    QDoubleSpinBox *addNumber(QFormLayout *form, const char *label, const QString &key,
                              int decimals, double minimum, double maximum, double defaultValue);
    void restoreChoice(QComboBox *combo, const QString &key, int defaultIndex);
    MeasurementSetup measurementSetup() const;
    void showReading(const Reading &reading);
    void clearReading();
    bool isFrequencyMode() const;
    QWidget *createFrequencyPlot();
    void setFrequencyPlotTheme();
    void appendFrequency(const Reading &reading);
    void clearFrequencyHistory();
    void updateFrequencyPlot();

    void editConnection();
    void saveSettings();
    void setDarkTheme(bool dark);
    void showAbout();
    void checkForUpdates();
    void checkPrivateRelease();
    void finishUpdateCheck(const QByteArray &data, const QString &error = {});
    void updateTargetLabel();
    void updateControls();
    void updateModelModes();
    void startMeasurement(Operation operation);
    void recall(const QString &label, const char *command, const char *code, const QString &unit = {});
    void run(Request request);
    void finished();

    QSettings preferences;
    GpibTarget target;
    bool darkTheme;

    QTranslator appTranslator;
    QTranslator qtTranslator;
    QString language;
    QMenu *languageMenu = nullptr;
    QAction *updateAction = nullptr;
    bool updateCheckInProgress = false;
    QTabWidget *tabs = nullptr;
    std::optional<Reading> displayedReading;
    std::deque<QPointF> frequencyHistory;
    QElapsedTimer frequencyClock;
    int frequencyHistoryMode = -1;
    QAction *clearFrequencyAction = nullptr;
    QWidget *frequencyPanel = nullptr;
    QwtPlot *frequencyPlot = nullptr;
    QwtPlotGrid *frequencyGrid = nullptr;
    QwtPlotCurve *frequencyCurve = nullptr;
    QAction *connectionSettings = nullptr;
    QAction *presetAction = nullptr;
    QMenu *recallMenu = nullptr;
    QLabel *targetLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *valueLabel = nullptr;
    QLabel *unitLabel = nullptr;
    QPushButton *connectButton = nullptr;
    QPushButton *singleButton = nullptr;
    QPushButton *continuousButton = nullptr;
    QPushButton *countButton = nullptr;
    QPushButton *readCountButton = nullptr;
    QPushButton *stopButton = nullptr;
    QPushButton *resetButton = nullptr;
    QPushButton *applyButton = nullptr;
    QWidget *measurementForm = nullptr;
    QWidget *inputTab = nullptr;
    QWidget *advancedTab = nullptr;
    QComboBox *mode = nullptr;
    QComboBox *resolution = nullptr;
    QComboBox *linkInputs = nullptr;
    QComboBox *filterA = nullptr;
    QComboBox *arming = nullptr;
    QComboBox *autoTrigger = nullptr;
    QComboBox *peakLevel = nullptr;
    QComboBox *selfTest = nullptr;
    QComboBox *serviceRequest = nullptr;
    QComboBox *delayMode = nullptr;
    QDoubleSpinBox *delay = nullptr;
    QComboBox *mathMode = nullptr;
    QLineEdit *mathX = nullptr;
    QLineEdit *mathZ = nullptr;
    std::array<InputControls, 2> inputs{};
    QList<QComboBox *> choices;
    QList<QDoubleSpinBox *> numbers;

    QThreadPool deviceThread;
    RacalDana device;
    QFutureWatcher<DeviceResult> watcher;
    QTimer readTimer;
    bool connected = false;
    int model = 0;
    bool busy = false;
    bool stopRequested = false;
    bool closeRequested = false;
    RunState runState = RunState::Idle;
    Operation operationInProgress = Operation::Connect;
    QString recallLabel;
    QString recallUnit;
    bool recallRegister = false;
};
