#include "mainwindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QMovie>
#include <QPalette>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <stdexcept>

namespace {

// Match the TR4131 language menu icon in both palettes.
QIcon globeIcon(bool dark)
{
    QPixmap icon(24, 24);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(dark ? "#d4e3f5" : "#27425a"), 1.35));
    painter.drawEllipse(QRectF(3, 3, 18, 18));
    painter.drawEllipse(QRectF(8, 3, 8, 18));
    painter.drawLine(QPointF(3, 12), QPointF(21, 12));
    painter.drawLine(QPointF(5, 7.5), QPointF(19, 7.5));
    painter.drawLine(QPointF(5, 16.5), QPointF(19, 16.5));
    return QIcon(icon);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      target{
          std::clamp(preferences.value(QStringLiteral("gpib/board"), 0).toInt(), 0, 255),
          std::clamp(preferences.value(QStringLiteral("gpib/address"), 1).toInt(), 0, 30)
      },
      darkTheme(preferences.value(QStringLiteral("appearance/dark"), true).toBool())
{
    setWindowTitle(qApp->applicationDisplayName());
    resize(900, 620);
    deviceThread.setMaxThreadCount(1);
    deviceThread.setExpiryTimeout(-1);
    readTimer.setSingleShot(true);
    connect(&readTimer, &QTimer::timeout, this, [this] { run(Request(Operation::Read)); });
    connect(&watcher, &QFutureWatcher<DeviceResult>::finished, this, &MainWindow::finished);
    installLanguage(preferences.value(QStringLiteral("language"), QStringLiteral("de")).toString());
    buildInterface();
}

void MainWindow::buildInterface()
{
    choices.clear();
    numbers.clear();
    createMenus();

    auto *workspace = new QWidget;
    setCentralWidget(workspace);
    auto *layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(22, 16, 22, 16);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel(qApp->applicationDisplayName());
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    heading->addWidget(title);
    targetLabel = new QLabel;
    targetLabel->setObjectName(QStringLiteral("gpibTarget"));
    heading->addWidget(targetLabel);
    header->addLayout(heading);
    header->addStretch();
    connectButton = new QPushButton(connected ? tr("Trennen") : tr("Verbinden"));
    connectButton->setObjectName(QStringLiteral("connectDevice"));
    header->addWidget(connectButton, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    tabs = new QTabWidget;
    tabs->addTab(createMeasurementTab(), tr("Messung"));
    inputTab = createInputTab();
    tabs->addTab(inputTab, tr("Eingänge"));
    advancedTab = createAdvancedTab();
    tabs->addTab(advancedTab, tr("Erweitert"));
    layout->addWidget(tabs, 1);

    auto *footer = new QHBoxLayout;
    statusLabel = new QLabel(tr("Getrennt"));
    statusLabel->setObjectName(QStringLiteral("connectionStatus"));
    statusLabel->setTextFormat(Qt::PlainText);
    statusLabel->setWordWrap(true);
    footer->addWidget(statusLabel, 1);
    applyButton = new QPushButton(tr("Einstellungen ans Gerät senden"));
    footer->addWidget(applyButton, 0, Qt::AlignVCenter);
    layout->addLayout(footer);

    connect(connectButton, &QPushButton::clicked, this, [this] {
        Request request(connected ? Operation::Disconnect : Operation::Connect);
        request.target = target;
        run(std::move(request));
    });
    connect(applyButton, &QPushButton::clicked, this, [this] { startMeasurement(Operation::Apply); });

    updateTargetLabel();
    updateModelModes();
    setDarkTheme(darkTheme);
    updateControls();
}

MainWindow::~MainWindow()
{
    readTimer.stop();
    deviceThread.waitForDone();
    qApp->removeTranslator(&appTranslator);
    qApp->removeTranslator(&qtTranslator);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!busy && !connected) {
        QMainWindow::closeEvent(event);
        return;
    }
    event->ignore();
    closeRequested = true;
    readTimer.stop();
    if (!busy) {
        run(Request(Operation::Disconnect));
    }
    updateControls();
}

void MainWindow::createMenus()
{
    auto *bar = new QMenuBar;
    bar->setNativeMenuBar(false);
    setMenuBar(bar);
    auto *file = bar->addMenu(tr("&Datei"));
    clearFrequencyAction = file->addAction(tr("Frequenzverlauf leeren"), this, &MainWindow::clearFrequencyHistory);
    clearFrequencyAction->setObjectName(QStringLiteral("clearFrequencyHistory"));
    file->addSeparator();
    file->addAction(tr("&Beenden"), QKeySequence::Quit, this, &QWidget::close);

    auto *settings = bar->addMenu(tr("&Einstellungen"));
    connectionSettings = settings->addAction(tr("Verbindung …"), this, &MainWindow::editConnection);
    connectionSettings->setObjectName(QStringLiteral("connectionSettings"));

    auto *appearance = settings->addMenu(tr("Darstellung"));
    auto *themes = new QActionGroup(appearance);
    for (bool dark : {false, true}) {
        auto *action = appearance->addAction(dark ? tr("Dunkel") : tr("Hell"));
        action->setObjectName(dark ? QStringLiteral("darkTheme") : QStringLiteral("lightTheme"));
        action->setCheckable(true);
        action->setChecked(dark == darkTheme);
        themes->addAction(action);
        connect(action, &QAction::triggered, this, [this, dark] { setDarkTheme(dark); });
    }
    settings->addSeparator();
    settings->addAction(tr("Einstellungen speichern"), QKeySequence::Save, this, &MainWindow::saveSettings)
        ->setObjectName(QStringLiteral("saveSettings"));

    auto *instrument = bar->addMenu(tr("&Gerät"));
    presetAction = instrument->addAction(tr("Einschaltzustand wiederherstellen …"), this, [this] {
        if (QMessageBox::question(this, tr("Geräte-Preset"),
            tr("Das Gerät auf den Einschaltzustand zurücksetzen?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            run(Request(Operation::Preset));
        }
    });
    recallMenu = instrument->addMenu(tr("Gerätedaten auslesen"));
    const auto addRecall = [this](const char *label, const char *command, const char *reply, const char *unit = "") {
        const QString title = tr(label);
        recallMenu->addAction(title, this, [this, title, command, reply, unit] {
            recall(title, command, reply, *unit ? tr(unit) : QString());
        });
    };
    addRecall(QT_TR_NOOP("Gerätekennung"), "RUT", "UT");
    addRecall(QT_TR_NOOP("Firmware Hauptgerät"), "RMS", "MS");
    addRecall(QT_TR_NOOP("Firmware GPIB"), "RGS", "GS");
    addRecall(QT_TR_NOOP("Auflösung"), "RRS", "RS", QT_TR_NOOP("Stellen"));
    addRecall(QT_TR_NOOP("Pegel Kanal A"), "RLA", "LA", "V");
    addRecall(QT_TR_NOOP("Pegel Kanal B"), "RLB", "LB", "V");
    addRecall(QT_TR_NOOP("Mathematikkonstante X"), "RMX", "MX");
    addRecall(QT_TR_NOOP("Mathematikkonstante Z"), "RMZ", "MZ");
    addRecall(QT_TR_NOOP("Stoppverzögerung"), "RDT", "DT", "s");
    addRecall(QT_TR_NOOP("Sonderfunktionsregister"), "RSF", "SF");

    auto *right = new QMenuBar(bar);
    right->setNativeMenuBar(false);
    languageMenu = new QMenu(tr("Sprache"), right);
    languageMenu->setObjectName(QStringLiteral("languageMenu"));
    right->addMenu(languageMenu);
    languageMenu->menuAction()->setText({});
    languageMenu->menuAction()->setToolTip(tr("Sprache wählen"));
    auto *languages = new QActionGroup(languageMenu);
    const std::array languagesAndNames{
        std::pair{QStringLiteral("de"), QStringLiteral("Deutsch")},
        std::pair{QStringLiteral("en"), QStringLiteral("English")},
        std::pair{QStringLiteral("fr"), QStringLiteral("Français")}
    };
    for (const auto &[code, label] : languagesAndNames) {
        auto *action = languageMenu->addAction(label);
        action->setData(code);
        action->setCheckable(true);
        action->setChecked(code == language);
        languages->addAction(action);
        connect(action, &QAction::triggered, this, [this, code] { changeLanguage(code); });
    }

    auto *about = right->addMenu(tr("Über"));
    about->addAction(tr("Über Qt"), this, [this] {
        QMessageBox::aboutQt(this, tr("Über Qt"));
    });
    about->addAction(tr("Über"), this, &MainWindow::showAbout)
        ->setObjectName(QStringLiteral("aboutApplication"));
    about->addSeparator();
    updateAction = about->addAction(tr("Nach Updates suchen"), this, &MainWindow::checkForUpdates);
    updateAction->setObjectName(QStringLiteral("checkForUpdates"));
    updateAction->setEnabled(!updateCheckInProgress);
    bar->setCornerWidget(right, Qt::TopRightCorner);
}

void MainWindow::installLanguage(const QString &code)
{
    language = code == QStringLiteral("en") || code == QStringLiteral("fr") ? code : QStringLiteral("de");
    const QLocale selectedLocale(language);
    QLocale::setDefault(selectedLocale);
    setLocale(selectedLocale);

    qApp->removeTranslator(&appTranslator);
    qApp->removeTranslator(&qtTranslator);
    if (qtTranslator.load(QStringLiteral(":/qt-i18n/qtbase_") + language)) {
        qApp->installTranslator(&qtTranslator);
    }
    if (appTranslator.load(QStringLiteral(":/i18n/racaldana_") + language)) {
        qApp->installTranslator(&appTranslator);
    }
}

void MainWindow::changeLanguage(const QString &code)
{
    if (code == language || busy || closeRequested) {
        return;
    }
    const int nextRead = readTimer.remainingTime();
    readTimer.stop();

    QList<int> selectedChoices;
    QList<double> selectedNumbers;
    for (const auto *choice : choices) {
        selectedChoices.append(choice->currentIndex());
    }
    for (const auto *number : numbers) {
        selectedNumbers.append(number->value());
    }
    const QString x = mathX->text();
    const QString z = mathZ->text();
    const int previousTab = tabs->currentIndex();
    const auto reading = displayedReading;

    installLanguage(code);
    preferences.setValue(QStringLiteral("language"), language);

    takeCentralWidget()->deleteLater();
    buildInterface();
    for (qsizetype i = 0; i < choices.size(); ++i) {
        choices.at(i)->setCurrentIndex(selectedChoices.at(i));
    }
    for (qsizetype i = 0; i < numbers.size(); ++i) {
        numbers.at(i)->setValue(selectedNumbers.at(i));
    }
    mathX->setText(x);
    mathZ->setText(z);
    tabs->setCurrentIndex(previousTab);
    updateModelModes();
    if (reading) {
        showReading(*reading);
    }

    switch (runState) {
    case RunState::Idle:
        statusLabel->setText(connected ? tr("Verbunden · %1").arg(targetLabel->text()) : tr("Getrennt"));
        break;
    case RunState::Single:
        statusLabel->setText(tr("Warte auf den Messwert …"));
        break;
    case RunState::Continuous:
        statusLabel->setText(reading ? tr("Dauermessung läuft.") : tr("Dauermessung läuft. Warte auf den ersten Messwert …"));
        break;
    case RunState::Counting:
        statusLabel->setText(tr("Summierung läuft."));
        break;
    case RunState::Diagnostic:
        statusLabel->setText(tr("Selbsttest läuft am Gerät."));
        break;
    }

    if (nextRead >= 0) {
        readTimer.start(nextRead);
    }
}

void MainWindow::editConnection()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("GPIB-Verbindung"));
    dialog.setMinimumWidth(390);
    auto *form = new QFormLayout(&dialog);
    form->addRow(tr("Schnittstelle"), new QLabel(QStringLiteral("NI-488.2")));

    auto *board = new QSpinBox;
    board->setObjectName(QStringLiteral("gpibBoard"));
    board->setRange(0, 255);
    board->setValue(target.board);
    form->addRow(tr("GPIB-&Bus"), board);

    auto *address = new QSpinBox;
    address->setObjectName(QStringLiteral("gpibAddress"));
    address->setRange(0, 30);
    address->setValue(target.address);
    form->addRow(tr("&Primäradresse"), address);

    auto *buttons = new QDialogButtonBox;
    buttons->addButton(tr("Abbrechen"), QDialogButtonBox::RejectRole);
    buttons->addButton(tr("Übernehmen"), QDialogButtonBox::AcceptRole);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        if (target.board != board->value() || target.address != address->value()) {
            clearFrequencyHistory();
        }
        target = {board->value(), address->value()};
        updateTargetLabel();
        statusLabel->clear();
    }
}

void MainWindow::saveSettings()
{
    preferences.setValue(QStringLiteral("gpib/board"), target.board);
    preferences.setValue(QStringLiteral("gpib/address"), target.address);
    preferences.setValue(QStringLiteral("appearance/dark"), darkTheme);
    for (const auto *combo : choices) {
        preferences.setValue(combo->objectName(), combo->currentIndex());
    }
    for (const auto *spin : numbers) {
        preferences.setValue(spin->objectName(), spin->value());
    }
    preferences.setValue(QStringLiteral("device/mathX"), mathX->text());
    preferences.setValue(QStringLiteral("device/mathZ"), mathZ->text());
    preferences.sync();
    statusLabel->setText(preferences.status() == QSettings::NoError
        ? tr("Einstellungen gespeichert.")
        : tr("Die Einstellungen konnten nicht gespeichert werden."));
}

void MainWindow::setDarkTheme(bool dark)
{
    darkTheme = dark;
    const auto color = [dark](const char *night, const char *day) {
        return QColor(dark ? night : day);
    };
    QPalette palette = qApp->style()->standardPalette();
    const QColor text = color("#e8f0fb", "#1c3044");
    palette.setColor(QPalette::Window, color("#10151e", "#edf3f8"));
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, color("#171f2b", "#ffffff"));
    palette.setColor(QPalette::AlternateBase, color("#192331", "#f9fbfd"));
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, color("#222e3e", "#e7eef5"));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Highlight, color("#33536d", "#cbe3f7"));
    palette.setColor(QPalette::HighlightedText, color("#e8f0fb", "#17334e"));
    palette.setColor(QPalette::ToolTipBase, palette.color(QPalette::AlternateBase));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Link, color("#99dbff", "#155785"));

    const QColor muted = color("#a3b3c8", "#496177");
    palette.setColor(QPalette::PlaceholderText, muted);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, muted);
    }
    qApp->setPalette(palette);
    qApp->setStyleSheet(QStringLiteral(R"(
        QMenuBar, QMenu { background: palette(alternate-base); color: palette(window-text); }
        QMenuBar::item { background: transparent; padding: 5px 11px; }
        QMenu::item { padding: 6px 24px; }
        QMenuBar::item:selected, QMenu::item:selected {
            background: palette(highlight); color: palette(highlighted-text);
        }
        QMenu::item:disabled { color: palette(placeholder-text); }
    )"));
    languageMenu->setIcon(globeIcon(dark));
    setFrequencyPlotTheme();
}

void MainWindow::showAbout()
{
    QMessageBox about(this);
    about.setWindowTitle(tr("Über"));
    about.setTextFormat(Qt::RichText);
    about.setText(tr("<b>%1</b><br>Version %2")
        .arg(qApp->applicationDisplayName(), qApp->applicationVersion()));
    QMovie logo(QStringLiteral(":/branding/logo_g.gif"));
    if (logo.isValid()) {
        connect(&logo, &QMovie::frameChanged, &about, [&] {
            about.setIconPixmap(logo.currentPixmap());
        });
        logo.start();
    } else {
        about.setIconPixmap(windowIcon().pixmap(120));
    }
    about.exec();
}

void MainWindow::updateTargetLabel()
{
    const QString bus = tr("NI-488.2 · GPIB %1 · Adresse %2").arg(target.board).arg(target.address);
    targetLabel->setText(model ? tr("Modell %1 · %2").arg(model).arg(bus) : bus);
}

void MainWindow::startMeasurement(Operation operation)
{
    Request request(operation);
    try {
        request.setup = measurementSetup();
        if (operation == Operation::Single && request.setup.diagnostic) {
            request.operation = Operation::Diagnostic;
        }
    } catch (const std::runtime_error &error) {
        statusLabel->setText(QString::fromUtf8(error.what()));
        return;
    }
    run(std::move(request));
}

void MainWindow::recall(const QString &label, const char *command, const char *code, const QString &unit)
{
    recallLabel = label;
    recallUnit = unit;
    recallRegister = QByteArrayView(command) == "RSF";
    Request request(Operation::Recall);
    request.recallCommand = command;
    request.recallCode = code;
    run(std::move(request));
}

void MainWindow::run(Request request)
{
    if (busy) {
        return;
    }
    readTimer.stop();
    busy = true;
    operationInProgress = request.operation;
    switch (operationInProgress) {
    case Operation::Connect:
        statusLabel->setText(tr("GPIB-Verbindung wird geöffnet …"));
        break;
    case Operation::Disconnect:
        statusLabel->setText(tr("Rückgabe an die Gerätebedienung …"));
        break;
    case Operation::Apply:
    case Operation::Single:
    case Operation::Continuous:
    case Operation::Diagnostic:
    case Operation::StartCount:
        statusLabel->setText(tr("Gerät wird eingestellt …"));
        break;
    case Operation::Stop:
        statusLabel->setText(tr("Messung wird gestoppt …"));
        break;
    case Operation::Recall:
        statusLabel->setText(tr("Gerätedaten werden gelesen …"));
        break;
    case Operation::ReadCount:
        statusLabel->setText(tr("Zwischenstand wird gelesen …"));
        break;
    default:
        break;
    }
    updateControls();
    watcher.setFuture(QtConcurrent::run(&deviceThread, [this, request = std::move(request)] {
        return device.execute(request);
    }));
}

void MainWindow::finished()
{
    const DeviceResult result = watcher.result();
    connected = result.connected;
    runState = result.runState;
    busy = false;
    if (model != result.model) {
        model = result.model;
        updateModelModes();
        updateTargetLabel();
    }
    connectButton->setText(connected ? tr("Trennen") : tr("Verbinden"));

    if (!result.error.isEmpty()) {
        statusLabel->setText(result.error);
    } else {
        if (result.reading && operationInProgress != Operation::Recall) {
            showReading(*result.reading);
            appendFrequency(*result.reading);
        }
        switch (operationInProgress) {
        case Operation::Connect:
            statusLabel->setText(tr("Verbunden · %1").arg(targetLabel->text()));
            break;
        case Operation::Disconnect:
            clearReading();
            statusLabel->setText(tr("Getrennt. Bedienung am Gerät wieder möglich."));
            break;
        case Operation::Apply:
            clearReading();
            statusLabel->setText(tr("Einstellungen ans Gerät übertragen."));
            break;
        case Operation::Single:
            clearReading();
            statusLabel->setText(tr("Warte auf den Messwert …"));
            break;
        case Operation::Continuous:
            clearReading();
            statusLabel->setText(tr("Dauermessung läuft. Warte auf den ersten Messwert …"));
            break;
        case Operation::Diagnostic:
            clearReading();
            statusLabel->setText(tr("Selbsttest läuft am Gerät."));
            break;
        case Operation::StartCount:
            statusLabel->setText(tr("Summierung läuft."));
            break;
        case Operation::Stop:
            statusLabel->setText(result.reading ? tr("Endstand empfangen.") : tr("Messung gestoppt."));
            break;
        case Operation::Read:
            if (result.reading) {
                statusLabel->setText(runState == RunState::Continuous
                    ? tr("Dauermessung läuft.")
                    : tr("Messwert empfangen."));
            }
            break;
        case Operation::ReadCount:
            statusLabel->setText(tr("Zwischenstand empfangen. Summierung läuft."));
            break;
        case Operation::Reset:
            clearReading();
            statusLabel->setText(tr("Messzyklus und Ausgabepuffer zurückgesetzt."));
            break;
        case Operation::Preset:
            clearReading();
            statusLabel->setText(tr("Einschaltzustand wiederhergestellt."));
            break;
        case Operation::Recall: {
            QLocale numberLocale = locale();
            numberLocale.setNumberOptions(QLocale::OmitGroupSeparator);
            const QString value = recallRegister
                ? QStringLiteral("%1").arg(qRound64(result.reading->value), 7, 10, QLatin1Char('0'))
                : numberLocale.toString(result.reading->value, 'g', 11);
            statusLabel->setText(QStringLiteral("%1 · %2 %3").arg(recallLabel, value, recallUnit).trimmed());
            break;
        }
        }
    }

    if (closeRequested) {
        if (connected) {
            run(Request(Operation::Disconnect));
        } else {
            close();
        }
        return;
    }
    if (std::exchange(stopRequested, false) && connected && runState != RunState::Idle) {
        run(Request(Operation::Stop));
        return;
    }
    updateControls();
    if (runState == RunState::Single || runState == RunState::Continuous) {
        readTimer.start(120);
    }
}
