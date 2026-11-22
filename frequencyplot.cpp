#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr std::size_t historyCapacity = 1000;

class LocaleScaleDraw final : public QwtScaleDraw
{
public:
    explicit LocaleScaleDraw(QLocale numberLocale) : numberLocale(numberLocale)
    {
        this->numberLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    }

    QwtText label(double value) const override
    {
        return QwtText(numberLocale.toString(value, 'g', 13));
    }

private:
    QLocale numberLocale;
};

} // namespace

QWidget *MainWindow::createFrequencyPlot()
{
    frequencyPanel = new QFrame;
    auto *layout = new QVBoxLayout(frequencyPanel);
    layout->setContentsMargins(0, 6, 0, 0);
    auto *heading = new QLabel(tr("Frequenzverlauf"));
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    frequencyPlot = new QwtPlot;
    frequencyPlot->setObjectName(QStringLiteral("frequencyPlot"));
    frequencyPlot->setMinimumHeight(220);
    frequencyPlot->setAutoFillBackground(true);
    if (auto *canvas = qobject_cast<QwtPlotCanvas *>(frequencyPlot->canvas())) {
        canvas->setFrameStyle(QFrame::NoFrame);
    }
    frequencyPlot->setAxisTitle(QwtAxis::XBottom, tr("Zeit seit Beginn [s]"));
    frequencyPlot->setAxisScaleDraw(QwtAxis::XBottom, new LocaleScaleDraw(locale()));
    frequencyPlot->setAxisScaleDraw(QwtAxis::YLeft, new LocaleScaleDraw(locale()));
    frequencyPlot->setAxisMaxMajor(QwtAxis::XBottom, 7);
    frequencyPlot->setAxisMaxMajor(QwtAxis::YLeft, 6);

    frequencyGrid = new QwtPlotGrid;
    frequencyGrid->attach(frequencyPlot);
    frequencyCurve = new QwtPlotCurve;
    frequencyCurve->setRenderHint(QwtPlotItem::RenderAntialiased);
    frequencyCurve->attach(frequencyPlot);
    layout->addWidget(frequencyPlot, 1);
    updateFrequencyPlot();
    return frequencyPanel;
}

void MainWindow::setFrequencyPlotTheme()
{
    const auto color = [this](const char *night, const char *day) {
        return QColor(darkTheme ? night : day);
    };
    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Window, color("#171f2b", "#f9fbfd"));
    palette.setColor(QPalette::WindowText, color("#d4e3f5", "#253f56"));
    palette.setColor(QPalette::Text, palette.color(QPalette::WindowText));
    frequencyPlot->setPalette(palette);
    for (const auto axis : {QwtAxis::XBottom, QwtAxis::YLeft}) {
        frequencyPlot->axisWidget(axis)->setPalette(palette);
    }
    frequencyPlot->setCanvasBackground(color("#101925", "#ffffff"));
    frequencyGrid->setMajorPen(color("#41576e", "#b0c2d0"), 1, Qt::DotLine);
    const QColor accent = color("#77ccff", "#006ea8");
    frequencyCurve->setPen(accent, 1.5);
    frequencyCurve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse, QBrush(accent), QPen(Qt::NoPen), QSize(5, 5)));
    frequencyPlot->replot();
}

void MainWindow::appendFrequency(const Reading &reading)
{
    if (!isFrequencyMode()) {
        return;
    }
    if (frequencyHistoryMode != mode->currentIndex()) {
        frequencyHistory.clear();
        frequencyHistoryMode = mode->currentIndex();
    }
    double seconds = 0.0;
    if (frequencyHistory.empty()) {
        frequencyClock.start();
    } else {
        seconds = frequencyClock.nsecsElapsed() / 1e9;
    }
    frequencyHistory.emplace_back(seconds, reading.value);
    if (frequencyHistory.size() > historyCapacity) {
        frequencyHistory.pop_front();
    }
    updateFrequencyPlot();
}

void MainWindow::clearFrequencyHistory()
{
    frequencyHistory.clear();
    updateFrequencyPlot();
}

void MainWindow::updateFrequencyPlot()
{
    const bool visible = isFrequencyMode();
    frequencyPanel->setVisible(visible);
    clearFrequencyAction->setEnabled(!frequencyHistory.empty());
    if (!visible) {
        return;
    }

    // Store values in Hz and use one SI unit for the entire displayed series.
    if (frequencyHistory.empty() || frequencyHistoryMode != mode->currentIndex()) {
        frequencyCurve->setSamples(QVector<QPointF>{});
        frequencyPlot->setAxisTitle(QwtAxis::YLeft, tr("Frequenz [%1]").arg(QStringLiteral("Hz")));
        frequencyPlot->setAxisScale(QwtAxis::XBottom, 0, 10);
        frequencyPlot->setAxisScale(QwtAxis::YLeft, 0, 1);
        frequencyPlot->replot();
        return;
    }

    const auto [lowest, highest] = std::minmax_element(
        frequencyHistory.begin(), frequencyHistory.end(),
        [](const QPointF &a, const QPointF &b) { return a.y() < b.y(); });
    const double magnitude = std::max(std::abs(lowest->y()), std::abs(highest->y()));
    double divisor = 1.0;
    QString unit = QStringLiteral("Hz");
    for (const auto &[scale, symbol] : {
         std::pair{1e9, "GHz"}, std::pair{1e6, "MHz"}, std::pair{1e3, "kHz"}}) {
        if (magnitude >= scale) {
            divisor = scale;
            unit = QString::fromLatin1(symbol);
            break;
        }
    }

    QVector<QPointF> samples;
    samples.reserve(static_cast<qsizetype>(frequencyHistory.size()));
    for (const auto &point : frequencyHistory) {
        samples.append(QPointF(point.x(), point.y() / divisor));
    }
    frequencyCurve->setSamples(samples);
    frequencyPlot->setAxisTitle(QwtAxis::YLeft, tr("Frequenz [%1]").arg(unit));
    const double start = samples.front().x();
    frequencyPlot->setAxisScale(QwtAxis::XBottom, start, std::max(start + 2.0, samples.back().x() + 0.2));
    const double low = lowest->y() / divisor;
    const double high = highest->y() / divisor;
    const double padding = std::max({(high - low) * 0.08, magnitude / divisor * 1e-11, 1e-12});
    frequencyPlot->setAxisScale(QwtAxis::YLeft, low - padding, high + padding);
    frequencyPlot->replot();
}
