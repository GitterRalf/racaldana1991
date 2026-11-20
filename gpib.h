#pragma once

#include <QByteArray>

#include <memory>

struct GpibTarget
{
    int board = 0;
    int address = 1;
};

class GpibSession
{
public:
    virtual ~GpibSession() = default;
    virtual void local() = 0;
    virtual void write(const QByteArray &commands) = 0;
    virtual QByteArray read(bool waitForReply = false) = 0;
    virtual unsigned char status() = 0;
};

std::unique_ptr<GpibSession> openGpib(GpibTarget target);
