#include "racaldana.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

#include <cmath>
#include <stdexcept>

namespace {

[[noreturn]] void fail(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}

// Handbuch 5.1/5.2: zwei Funktionszeichen, 17 Zeichen für die Zahl, CR + LF.
// Bei aktiver Sonderfunktion kann das Funktionsfeld aus Leerzeichen bestehen.
Reading parseReading(const QByteArray &frame, const QByteArray &expected)
{
    static const QRegularExpression numberPattern(QStringLiteral(R"(\A[+-][0-9][0-9.]{11}E[+-][0-9]{2}\z)"));
    const QByteArray function = frame.left(2);
    const QString number = QString::fromLatin1(frame.mid(2, 17));
    bool ok = false;
    const double value = number.toDouble(&ok);
    if (frame.size() != 21 || !frame.endsWith("\r\n")
        || (function != expected && function != "  ")
        || !numberPattern.match(number).hasMatch() || number.count(QLatin1Char('.')) != 1
        || !ok || !std::isfinite(value)) {
        fail(QCoreApplication::translate("RacalDana", "Unerwartete Antwort für %1 (%2).")
            .arg(QString::fromLatin1(expected), QString::fromLatin1(frame.toHex(' ').left(80))));
    }
    // Tabelle 4.4: Er 50 ist ein fehlgeschlagener CHECK, kein Frequenzwert.
    if (expected == "CK" && value == 50.0) {
        fail(QCoreApplication::translate("RacalDana", "Geräte-Selbsttest fehlgeschlagen (Er 50)."));
    }
    return {number, value};
}

} // namespace

DeviceResult RacalDana::execute(const Request &request)
{
    DeviceResult result;
    try {
        if (request.operation != Operation::Connect && !session) {
            fail(QCoreApplication::translate("RacalDana", "Keine GPIB-Verbindung."));
        }
        switch (request.operation) {
        case Operation::Connect: {
            session = openGpib(request.target);
            session->write("RUT");
            const double identity = read("UT", true)->value;
            if (identity != 1991 && identity != 1992) {
                fail(QCoreApplication::translate("RacalDana", "Unerwartete Gerätekennung %1. Erwartet wird ein Racal-Dana 1991 oder 1992.").arg(identity));
            }
            model = static_cast<int>(identity);
            break;
        }
        case Operation::Disconnect:
            if (state == RunState::Counting) {
                session->write("T3");
            } else if (state != RunState::Idle) {
                session->write("T1 RE");
            }
            session->local();
            session.reset();
            appliedCommands.clear();
            state = RunState::Idle;
            break;
        case Operation::Apply:
            apply(request.setup);
            break;
        case Operation::Single:
        case Operation::Continuous:
            apply(request.setup);
            if (request.operation == Operation::Single) {
                command("T2");
                state = RunState::Single;
            } else {
                command("T0");
                state = RunState::Continuous;
            }
            break;
        case Operation::Diagnostic:
            apply(request.setup);
            command("T2");
            state = RunState::Diagnostic;
            break;
        case Operation::StartCount:
            // Der Zählerstand bleibt zwischen T3 und dem nächsten T2 erhalten.
            if (appliedCommands != request.setup.commands) {
                apply(request.setup);
            }
            command("T2");
            state = RunState::Counting;
            break;
        case Operation::ReadCount:
            command("RF");
            result.reading = read(expectedFunction, true);
            break;
        case Operation::Stop:
            if (state == RunState::Counting) {
                command("T3");
                state = RunState::Idle;
                result.reading = read(expectedFunction, true);
            } else {
                command("T1 RE");
                state = RunState::Idle;
            }
            break;
        case Operation::Read:
            result.reading = read(expectedFunction);
            if (result.reading && state != RunState::Continuous) {
                state = RunState::Idle;
            }
            break;
        case Operation::Reset:
            command("T1 RE");
            state = RunState::Idle;
            break;
        case Operation::Preset:
            command("IP");
            appliedCommands.clear();
            state = RunState::Idle;
            break;
        case Operation::Recall:
            command("T1");
            command(request.recallCommand);
            result.reading = read(request.recallCode, true);
            break;
        }
    } catch (const std::runtime_error &error) {
        result.error = QString::fromUtf8(error.what());
        if (request.operation == Operation::Connect || request.operation == Operation::Disconnect) {
            session.reset();
            state = RunState::Idle;
            appliedCommands.clear();
        } else if (request.operation != Operation::ReadCount && state != RunState::Counting) {
            if (state == RunState::Single || state == RunState::Continuous || state == RunState::Diagnostic) {
                try {
                    session->write("T1 RE");
                } catch (const std::runtime_error &) {
                    session.reset();
                }
            }
            state = RunState::Idle;
            appliedCommands.clear();
        }
    }
    result.connected = bool(session);
    result.model = session ? model : 0;
    result.runState = state;
    return result;
}

void RacalDana::apply(const MeasurementSetup &setup)
{
    command("T1 RE SFD");
    for (const auto &code : setup.commands.split(' ')) {
        command(code);
    }
    appliedCommands = setup.commands;
    expectedFunction = setup.function;
}

void RacalDana::command(const QByteArray &code)
{
    QByteArray wireCode = code;
    session->write(wireCode);
    auto status = session->status();
    // Das 1992 antwortet auf DD. DO steht in Tabelle 5.9 und ist die Alternative
    // für Firmware, die DD mit Syntaxfehler ablehnt.
    constexpr unsigned char errorFlag = 0x20;
    constexpr unsigned char errorNumberMask = 0x07;
    constexpr unsigned char syntaxError = 5;
    if (code == "DD" && (status & errorFlag) && (status & errorNumberMask) == syntaxError) {
        wireCode = "DO";
        session->write(wireCode);
        status = session->status();
    }
    checkStatus(status, false, wireCode);
}

std::optional<Reading> RacalDana::read(const QByteArray &expected, bool waitForReply)
{
    const QByteArray frame = session->read(waitForReply);
    if (!frame.isEmpty()) {
        return parseReading(frame, expected);
    }
    checkStatus(session->status(), true);
    if (waitForReply) {
        fail(QCoreApplication::translate("RacalDana", "Das Gerät hat keine Daten für %1 geliefert.").arg(QString::fromLatin1(expected)));
    }
    return std::nullopt;
}

void RacalDana::checkStatus(unsigned char status, bool reading, const QByteArray &command)
{
    // Handbuch 5.3: DID 6 meldet einen Fehler, DID 1–3 enthalten dessen Nummer.
    if ((status & 0x20) == 0) {
        return;
    }
    const int code = status & 0x07;
    if (!reading && code >= 1 && code <= 3) {
        return;
    }
    static const QStringList errors{
        QCoreApplication::translate("RacalDana", "Unbekannter Gerätefehler."),
        QCoreApplication::translate("RacalDana", "Phasenmessung bei unterschiedlichen Eingangsfrequenzen."),
        QCoreApplication::translate("RacalDana", "Messwert außerhalb des Anzeigebereichs."),
        QCoreApplication::translate("RacalDana", "Überlauf der internen Zähler."),
        QCoreApplication::translate("RacalDana", "Das Gerät hat eine numerische Eingabe abgelehnt."),
        QCoreApplication::translate("RacalDana", "Das Gerät hat einen GPIB-Befehl abgelehnt.")
    };
    const QString message = QCoreApplication::translate("RacalDana", "Gerätefehler %1 · %2").arg(code).arg(errors.value(code, errors.first()));
    fail(command.isEmpty() ? message : QCoreApplication::translate("RacalDana", "Befehl %1 · %2").arg(QString::fromLatin1(command), message));
}
