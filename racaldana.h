#pragma once

#include "gpib.h"

#include <QString>

#include <optional>

enum class Operation { Connect, Disconnect, Apply, Single, Continuous, Diagnostic, StartCount, ReadCount,
                       Stop, Read, Reset, Preset, Recall };
enum class RunState { Idle, Single, Continuous, Counting, Diagnostic };

struct MeasurementSetup
{
    QByteArray commands;
    QByteArray function;
    bool diagnostic = false;
};

struct Request
{
    explicit Request(Operation operation) : operation(operation) {}

    Operation operation;
    GpibTarget target;
    MeasurementSetup setup;
    QByteArray recallCommand;
    QByteArray recallCode;
};

struct Reading
{
    QString number;
    double value;
};

struct DeviceResult
{
    bool connected = false;
    int model = 0;
    RunState runState = RunState::Idle;
    std::optional<Reading> reading;
    QString error;
};

class RacalDana
{
public:
    DeviceResult execute(const Request &request);

private:
    void apply(const MeasurementSetup &setup);
    void command(const QByteArray &code);
    std::optional<Reading> read(const QByteArray &expected, bool waitForReply = false);
    static void checkStatus(unsigned char status, bool reading, const QByteArray &command = {});

    std::unique_ptr<GpibSession> session;
    int model = 0;
    RunState state = RunState::Idle;
    QByteArray appliedCommands;
    QByteArray expectedFunction;
};
