#include "mainwindow.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

struct Mode {
    const char *label;
    const char *code;
    const char *unit;
    bool swapped = false;
    bool manual = false;
    bool channelC = false;
};

constexpr Mode modes[]{
    {QT_TRANSLATE_NOOP("MainWindow", "Frequenz A"), "FA", "Hz"},
    {QT_TRANSLATE_NOOP("MainWindow", "Frequenz B"), "FA", "Hz", true},
    {QT_TRANSLATE_NOOP("MainWindow", "Periode A"), "PA", "s"},
    {QT_TRANSLATE_NOOP("MainWindow", "Periode B"), "PA", "s", true},
    {QT_TRANSLATE_NOOP("MainWindow", "Zeitintervall A → B"), "TI", "s"},
    {QT_TRANSLATE_NOOP("MainWindow", "Zeitintervall B → A"), "TI", "s", true},
    {QT_TRANSLATE_NOOP("MainWindow", "Ereignisse A durch B"), "TA", QT_TRANSLATE_NOOP("MainWindow", "Impulse")},
    {QT_TRANSLATE_NOOP("MainWindow", "Ereignisse B durch A"), "TA", QT_TRANSLATE_NOOP("MainWindow", "Impulse"), true},
    {QT_TRANSLATE_NOOP("MainWindow", "Manuell summieren A"), "TA", QT_TRANSLATE_NOOP("MainWindow", "Impulse"), false, true},
    {QT_TRANSLATE_NOOP("MainWindow", "Manuell summieren B"), "TA", QT_TRANSLATE_NOOP("MainWindow", "Impulse"), true, true},
    {QT_TRANSLATE_NOOP("MainWindow", "Phase A relativ zu B"), "PH", "°"},
    {QT_TRANSLATE_NOOP("MainWindow", "Phase B relativ zu A"), "PH", "°", true},
    {QT_TRANSLATE_NOOP("MainWindow", "Verhältnis A/B"), "RA", ""},
    {QT_TRANSLATE_NOOP("MainWindow", "Selbsttest"), "CK", "Hz"},
    {QT_TRANSLATE_NOOP("MainWindow", "Frequenz C"), "FC", "Hz", false, false, true},
    {QT_TRANSLATE_NOOP("MainWindow", "Verhältnis C/B"), "RC", "", false, false, true},
};

QByteArray code(const QComboBox *combo)
{
    return combo->currentData().toByteArray();
}

QByteArray mathNumber(const QLineEdit *field, const char *name)
{
    bool ok = false;
    const double value = field->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    constexpr double minMagnitude = 1e-9;
    constexpr double maxMagnitude = 1e10;
    const QByteArray encoded = QByteArray::number(value, 'G', 9);
    const double magnitude = std::abs(encoded.toDouble());
    if (!ok || !std::isfinite(value) || magnitude >= maxMagnitude
        || (value != 0.0 && magnitude <= minMagnitude) || (value == 0.0 && name[0] == 'Z')) {
        throw std::runtime_error(QCoreApplication::translate("MainWindow",
            "Ungültige Mathematikkonstante %1. Der Betrag muss zwischen 10⁻⁹ und 10¹⁰ liegen. "
            "Für X ist auch 0 zulässig.").arg(QString::fromLatin1(name)).toUtf8().constData());
    }
    return encoded;
}

} // namespace

QWidget *MainWindow::createMeasurementTab()
{
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    auto *display = new QFrame;
    display->setFrameShape(QFrame::StyledPanel);
    display->setAutoFillBackground(true);
    display->setBackgroundRole(QPalette::Base);
    auto *displayLayout = new QHBoxLayout(display);
    displayLayout->setContentsMargins(18, 10, 18, 10);
    displayLayout->addStretch();

    valueLabel = new QLabel;
    valueLabel->setObjectName(QStringLiteral("measurementValue"));
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    QFont valueFont(QStringLiteral("Consolas"), 29, QFont::DemiBold);
    valueLabel->setFont(valueFont);
    valueLabel->setForegroundRole(QPalette::Text);
    displayLayout->addWidget(valueLabel, 0, Qt::AlignBaseline);

    unitLabel = new QLabel;
    unitLabel->setForegroundRole(QPalette::Text);
    displayLayout->addWidget(unitLabel, 0, Qt::AlignBaseline);
    layout->addWidget(display);
    clearReading();

    measurementForm = new QWidget;
    auto *form = new QFormLayout(measurementForm);
    form->setContentsMargins(0, 0, 0, 0);
    mode = new QComboBox;
    for (const auto &item : modes) {
        mode->addItem(tr(item.label));
        if (item.channelC) {
            mode->setItemData(mode->count() - 1, tr("Nur Modell 1992"), Qt::ToolTipRole);
        }
    }
    restoreChoice(mode, QStringLiteral("device/mode"), 0);
    form->addRow(tr("Messfunktion"), mode);
    resolution = addChoice(form, QT_TR_NOOP("Auflösung"), QStringLiteral("device/resolution"), {
        {QT_TR_NOOP("3 Stellen"), "3"}, {QT_TR_NOOP("4 Stellen"), "4"}, {QT_TR_NOOP("5 Stellen"), "5"}, {QT_TR_NOOP("6 Stellen"), "6"},
        {QT_TR_NOOP("7 Stellen"), "7"}, {QT_TR_NOOP("8 Stellen"), "8"}, {QT_TR_NOOP("9 Stellen"), "9"}, {QT_TR_NOOP("10 Stellen"), "10"}
    }, 6);
    resolution->setToolTip(tr("Torzeit für Frequenz, Periode und Selbsttest. "
        "3–6 Stellen 1 ms, 7 Stellen 10 ms, 8 Stellen 100 ms, 9 Stellen 1 s, 10 Stellen 10 s."));
    layout->addWidget(measurementForm);

    auto *buttons = new QHBoxLayout;
    singleButton = new QPushButton(tr("Einzelmessung"));
    continuousButton = new QPushButton(tr("Dauermessung"));
    countButton = new QPushButton(tr("Summieren starten"));
    readCountButton = new QPushButton(tr("Zwischenstand lesen"));
    stopButton = new QPushButton(tr("Stopp"));
    resetButton = new QPushButton(tr("Messung zurücksetzen"));
    for (auto *button : {singleButton, continuousButton, countButton, readCountButton, stopButton, resetButton}) {
        buttons->addWidget(button);
    }
    buttons->addStretch();
    layout->addLayout(buttons);
    layout->addWidget(createFrequencyPlot(), 1);
    layout->addStretch(0);

    connect(mode, &QComboBox::currentIndexChanged, this, [this] {
        clearReading();
        updateFrequencyPlot();
        updateControls();
    });
    connect(singleButton, &QPushButton::clicked, this, [this] { startMeasurement(Operation::Single); });
    connect(continuousButton, &QPushButton::clicked, this, [this] { startMeasurement(Operation::Continuous); });
    connect(countButton, &QPushButton::clicked, this, [this] { startMeasurement(Operation::StartCount); });
    connect(readCountButton, &QPushButton::clicked, this, [this] { run(Request(Operation::ReadCount)); });
    connect(stopButton, &QPushButton::clicked, this, [this] {
        if (busy) {
            stopRequested = true;
            statusLabel->setText(tr("Messung wird gestoppt …"));
            updateControls();
        } else {
            run(Request(Operation::Stop));
        }
    });
    connect(resetButton, &QPushButton::clicked, this, [this] { run(Request(Operation::Reset)); });
    return tab;
}

QWidget *MainWindow::createInputTab()
{
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);
    auto *channels = new QHBoxLayout;
    for (size_t i = 0; i < inputs.size(); ++i) {
        const QString channel = i == 0 ? QStringLiteral("A") : QStringLiteral("B");
        const QString key = QStringLiteral("device/input%1/").arg(channel);
        auto *group = new QGroupBox(tr("Kanal %1").arg(channel));
        auto *form = new QFormLayout(group);
        InputControls &input = inputs.at(i);
        input.coupling = addChoice(form, QT_TR_NOOP("Kopplung"), key + QStringLiteral("coupling"),
            {{QT_TR_NOOP("Unverändert"), ""}, {"AC", "AC"}, {"DC", "DC"}});
        input.impedance = addChoice(form, QT_TR_NOOP("Impedanz"), key + QStringLiteral("impedance"),
            {{QT_TR_NOOP("Unverändert"), ""}, {"1 MΩ", "HI"}, {"50 Ω", "LI"}});
        input.slope = addChoice(form, QT_TR_NOOP("Triggerflanke"), key + QStringLiteral("slope"),
            {{QT_TR_NOOP("Unverändert"), ""}, {QT_TR_NOOP("Steigend"), "PS"}, {QT_TR_NOOP("Fallend"), "NS"}});
        input.attenuation = addChoice(form, QT_TR_NOOP("Abschwächer"), key + QStringLiteral("attenuation"),
            {{QT_TR_NOOP("Unverändert"), ""}, {"×1", "AD"}, {"×10", "AE"}});
        input.trigger = addChoice(form, QT_TR_NOOP("Trigger"), key + QStringLiteral("trigger"),
            {{QT_TR_NOOP("Unverändert"), ""}, {"Auto", "AU"}, {QT_TR_NOOP("Manuell"), "MN"}});
        const double range = code(input.attenuation) == "AE" ? 51.0 : 5.1;
        input.level = addNumber(form, QT_TR_NOOP("Triggerpegel"), key + QStringLiteral("level"), 2, -range, range, 0.0);
        input.level->setSuffix(QStringLiteral(" V"));
        input.level->setSingleStep(range > 5.1 ? 0.2 : 0.02);
        input.level->setEnabled(code(input.trigger) == "MN");
        connect(input.attenuation, &QComboBox::currentIndexChanged, this, [level = input.level, atten = input.attenuation] {
            const double range = code(atten) == "AE" ? 51.0 : 5.1;
            level->setRange(-range, range);
            level->setSingleStep(range > 5.1 ? 0.2 : 0.02);
        });
        connect(input.trigger, &QComboBox::currentIndexChanged, this, [level = input.level, trigger = input.trigger] {
            level->setEnabled(code(trigger) == "MN");
        });
        channels->addWidget(group, 1);
    }
    layout->addLayout(channels);

    auto *form = new QFormLayout;
    linkInputs = addChoice(form, QT_TR_NOOP("Eingänge A/B"), QStringLiteral("device/linkInputs"),
        {{QT_TR_NOOP("Unverändert"), ""}, {QT_TR_NOOP("Getrennte Eingänge"), "BCS"}, {QT_TR_NOOP("Gemeinsames Signal (COM A)"), "BCC"}});
    filterA = addChoice(form, QT_TR_NOOP("Tiefpass Kanal A"), QStringLiteral("device/filterA"),
        {{QT_TR_NOOP("Unverändert"), ""}, {QT_TR_NOOP("Aus"), "AFD"}, {QT_TR_NOOP("Ein"), "AFE"}});
    layout->addLayout(form);
    layout->addStretch();
    return tab;
}

QWidget *MainWindow::createAdvancedTab()
{
    auto *tab = new QWidget;
    auto *layout = new QHBoxLayout(tab);
    auto *functions = new QGroupBox(tr("Sonderfunktionen"));
    auto *special = new QFormLayout(functions);
    arming = addChoice(special, QT_TR_NOOP("Arming Start / Stopp"), QStringLiteral("device/arming"), {
        {QT_TR_NOOP("Intern / Intern"), "S10"}, {QT_TR_NOOP("Extern ↑ / Intern"), "S11"}, {QT_TR_NOOP("Extern ↓ / Intern"), "S12"},
        {QT_TR_NOOP("Intern / Extern ↑"), "S13"}, {QT_TR_NOOP("Intern / Extern ↓"), "S14"},
        {QT_TR_NOOP("Extern ↑ / Extern ↑"), "S15"}, {QT_TR_NOOP("Extern ↑ / Extern ↓"), "S16"},
        {QT_TR_NOOP("Extern ↓ / Extern ↑"), "S17"}, {QT_TR_NOOP("Extern ↓ / Extern ↓"), "S18"}
    });
    autoTrigger = addChoice(special, QT_TR_NOOP("Auto-Triggerpegel"), QStringLiteral("device/autoTrigger"),
        {{QT_TR_NOOP("Fortlaufend bestimmen"), "S30"}, {QT_TR_NOOP("Einmal bestimmen"), "S31"}});
    peakLevel = addChoice(special, QT_TR_NOOP("Pegel zum Auslesen"), QStringLiteral("device/peakLevel"),
        {{QT_TR_NOOP("Triggerpegel"), "S50"}, {QT_TR_NOOP("Positive Spitze"), "S51"}, {QT_TR_NOOP("Negative Spitze"), "S52"}});
    selfTest = addChoice(special, QT_TR_NOOP("Selbsttest"), QStringLiteral("device/selfTest"),
        {{QT_TR_NOOP("10-MHz-Referenz"), "S70"}, {"LEDs", "S71"}, {QT_TR_NOOP("Relais Kanal A"), "S77"}, {QT_TR_NOOP("Relais Kanal B"), "S78"}});
    serviceRequest = addChoice(special, QT_TR_NOOP("Service Request"), QStringLiteral("device/srq"), {
        {QT_TR_NOOP("Aus"), "Q0"}, {QT_TR_NOOP("Fehler"), "Q1"}, {QT_TR_NOOP("Messwert"), "Q2"}, {QT_TR_NOOP("Messwert / Fehler"), "Q3"},
        {QT_TR_NOOP("Zeitbasiswechsel"), "Q4"}, {QT_TR_NOOP("Zeitbasiswechsel / Fehler"), "Q5"},
        {QT_TR_NOOP("Zeitbasiswechsel / Messwert"), "Q6"}, {QT_TR_NOOP("Alle Ereignisse"), "Q7"}
    }, 1);

    auto *processing = new QGroupBox(tr("Messwert und Zeitsteuerung"));
    auto *form = new QFormLayout(processing);
    delayMode = addChoice(form, QT_TR_NOOP("Stoppverzögerung"), QStringLiteral("device/delayMode"),
        {{QT_TR_NOOP("Aus"), "DD"}, {QT_TR_NOOP("Ein"), "DE"}});
    delay = addNumber(form, QT_TR_NOOP("Verzögerung"), QStringLiteral("device/delay"), 4, 0.2048, 800.0, 1.0);
    delay->setSuffix(QStringLiteral(" ms"));
    delay->setSingleStep(0.0256);
    mathMode = addChoice(form, QT_TR_NOOP("Mathematik (Messwert − X) / Z"), QStringLiteral("device/mathMode"),
        {{QT_TR_NOOP("Aus"), "MD"}, {QT_TR_NOOP("Ein"), "ME"}});
    mathX = new QLineEdit(preferences.value(QStringLiteral("device/mathX"), QStringLiteral("0")).toString());
    mathZ = new QLineEdit(preferences.value(QStringLiteral("device/mathZ"), QStringLiteral("1")).toString());
    mathX->setMaxLength(24);
    mathZ->setMaxLength(24);
    form->addRow(tr("Konstante X"), mathX);
    form->addRow(tr("Konstante Z"), mathZ);
    layout->addWidget(functions, 1);
    layout->addWidget(processing, 1);
    layout->setAlignment(Qt::AlignTop);

    connect(selfTest, &QComboBox::currentIndexChanged, this, [this] { updateControls(); });
    connect(delayMode, &QComboBox::currentIndexChanged, this, [this] { updateControls(); });
    connect(mathMode, &QComboBox::currentIndexChanged, this, [this] { updateControls(); });
    return tab;
}

QComboBox *MainWindow::addChoice(QFormLayout *form, const char *label, const QString &key,
    std::initializer_list<Choice> options, int defaultIndex)
{
    auto *combo = new QComboBox;
    for (const auto &[text, data] : options) {
        combo->addItem(tr(text), QByteArray(data));
    }
    restoreChoice(combo, key, defaultIndex);
    form->addRow(tr(label), combo);
    return combo;
}

QDoubleSpinBox *MainWindow::addNumber(QFormLayout *form, const char *label, const QString &key, int decimals,
    double minimum, double maximum, double defaultValue)
{
    auto *spin = new QDoubleSpinBox;
    spin->setObjectName(key);
    spin->setDecimals(decimals);
    spin->setRange(minimum, maximum);
    spin->setValue(preferences.value(key, defaultValue).toDouble());
    numbers.append(spin);
    form->addRow(tr(label), spin);
    return spin;
}

void MainWindow::restoreChoice(QComboBox *combo, const QString &key, int defaultIndex)
{
    combo->setObjectName(key);
    combo->setCurrentIndex(std::clamp(preferences.value(key, defaultIndex).toInt(), 0, combo->count() - 1));
    choices.append(combo);
}

MeasurementSetup MainWindow::measurementSetup() const
{
    const Mode &selected = modes[mode->currentIndex()];
    const bool checkMode = QByteArrayView(selected.code) == "CK";
    const bool enableMath = !checkMode && QByteArrayView(selected.code) != "PH" && code(mathMode) == "ME";
    const QByteArray test = checkMode ? code(selfTest) : QByteArray("S70");
    const bool special = selected.swapped || selected.manual || arming->currentIndex() != 0
        || autoTrigger->currentIndex() != 0 || peakLevel->currentIndex() != 0 || checkMode;

    QByteArrayList settings{selected.code};
    for (size_t i = 0; i < inputs.size(); ++i) {
        const char channel = i == 0 ? 'A' : 'B';
        const InputControls &input = inputs.at(i);
        for (const auto *option : {input.coupling, input.impedance, input.slope, input.attenuation, input.trigger}) {
            if (!code(option).isEmpty()) {
                settings << channel + code(option);
            }
        }
        if (code(input.trigger) == "MN") {
            settings << QByteArray("SL") + channel + QByteArray::number(input.level->value(), 'f', 2);
        }
    }
    const QByteArray common = code(linkInputs);
    const QByteArray filter = code(filterA);
    if (!common.isEmpty()) {
        settings << common;
    }
    if (!filter.isEmpty()) {
        settings << filter;
    }

    settings << "SRS" + code(resolution);
    settings << code(delayMode);
    if (code(delayMode) == "DE") {
        settings << "SDT" + QByteArray::number(delay->value() / 1000.0, 'G', 9);
    }
    if (enableMath) {
        settings << "SMX" + mathNumber(mathX, "X") << "SMZ" + mathNumber(mathZ, "Z");
    }
    settings << (enableMath ? "ME" : "MD");
    if (special) {
        // Abschnitt 4.15/Tabelle 4.3: S61 wählt das manuelle Summieren.
        settings << code(arming) << (selected.swapped ? "S21" : "S20") << code(autoTrigger)
                 << code(peakLevel) << (selected.manual ? "S61" : "S60") << test << "SFE";
    }
    settings << code(serviceRequest);
    return {settings.join(' '), selected.code, test != "S70"};
}

bool MainWindow::isFrequencyMode() const
{
    const QByteArrayView function(modes[mode->currentIndex()].code);
    return function == "FA" || function == "FC";
}

void MainWindow::showReading(const Reading &reading)
{
    const Mode &selected = modes[mode->currentIndex()];
    displayedReading = reading;
    QString unit = *selected.unit ? tr(selected.unit) : QString();
    const QLocale numberLocale = locale();
    QString text;
    const int separator = reading.number.indexOf(QLatin1Char('E'));
    const int exponent = reading.number.mid(separator + 1).toInt();
    if ((unit == QStringLiteral("Hz") || unit == QStringLiteral("s")) &&
        exponent >= -15 && exponent <= 12 && exponent % 3 == 0) {
        static const QString prefixes[]{QStringLiteral("f"), QStringLiteral("p"), QStringLiteral("n"),
            QStringLiteral("µ"), QStringLiteral("m"), QString(), QStringLiteral("k"),
            QStringLiteral("M"), QStringLiteral("G"), QStringLiteral("T")};
        text = reading.number.mid(1, separator - 1);
        while (text.startsWith(QLatin1Char('0')) && text.at(1).isDigit()) {
            text.remove(0, 1);
        }
        if (reading.number.startsWith(QLatin1Char('-'))) {
            text.prepend(QLatin1Char('-'));
        }
        text.replace(QLatin1Char('.'), numberLocale.decimalPoint());
        unit.prepend(prefixes[(exponent + 15) / 3]);
    } else {
        QLocale displayLocale = numberLocale;
        displayLocale.setNumberOptions(QLocale::OmitGroupSeparator);
        text = displayLocale.toString(reading.value, 'g', 11);
    }
    valueLabel->setText(text);
    unitLabel->setText(unit);
    valueLabel->setToolTip(reading.number);
}

void MainWindow::clearReading()
{
    displayedReading.reset();
    valueLabel->setText(QStringLiteral("—"));
    valueLabel->setToolTip(QString());
    unitLabel->clear();
}

void MainWindow::updateModelModes()
{
    auto *items = qobject_cast<QStandardItemModel *>(mode->model());
    for (int i = 0; i < mode->count(); ++i) {
        if (modes[i].channelC) {
            items->item(i)->setEnabled(model == 1992);
        }
    }
    if (model == 1991 && modes[mode->currentIndex()].channelC) {
        mode->setCurrentIndex(0);
    }
}

void MainWindow::updateControls()
{
    const Mode &selected = modes[mode->currentIndex()];
    const bool idle = runState == RunState::Idle;
    const bool editable = idle && !busy && !closeRequested;
    const bool ready = connected && editable;
    const bool counting = runState == RunState::Counting;

    connectButton->setEnabled(!busy && !closeRequested);
    languageMenu->menuAction()->setEnabled(!busy && !closeRequested);
    connectionSettings->setEnabled(!connected && editable);
    presetAction->setEnabled(ready);
    recallMenu->setEnabled(ready);
    measurementForm->setEnabled(editable);
    inputTab->setEnabled(editable);
    advancedTab->setEnabled(editable);
    const bool checkMode = QByteArrayView(selected.code) == "CK";
    const bool diagnostic = checkMode && selfTest->currentIndex() != 0;
    selfTest->setEnabled(checkMode);
    mathMode->setEnabled(QByteArrayView(selected.code) != "PH" && QByteArrayView(selected.code) != "CK");
    mathX->setEnabled(mathMode->isEnabled() && code(mathMode) == "ME");
    mathZ->setEnabled(mathX->isEnabled());
    delay->setEnabled(code(delayMode) == "DE");
    applyButton->setEnabled(ready);
    singleButton->setVisible(!selected.manual);
    singleButton->setText(diagnostic ? tr("Selbsttest starten") : tr("Einzelmessung"));
    continuousButton->setVisible(!selected.manual && !diagnostic);
    countButton->setVisible(selected.manual);
    readCountButton->setVisible(selected.manual);
    for (auto *button : {singleButton, continuousButton, countButton, resetButton}) {
        button->setEnabled(ready);
    }
    readCountButton->setEnabled(counting && connected && !busy && !closeRequested);
    stopButton->setEnabled(!idle && (!busy || operationInProgress == Operation::Read)
        && !stopRequested && !closeRequested);
}
