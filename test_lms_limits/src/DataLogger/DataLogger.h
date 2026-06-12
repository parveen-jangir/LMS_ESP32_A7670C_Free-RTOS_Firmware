#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

struct PacketData
{
    String data;
    size_t packetSize;
    size_t startOffset;
    size_t endOffset;
};

struct LoggerInfo
{
    size_t currentSize;
    size_t maxSize;
    size_t uploadedOffset;
    size_t pendingBytes;
    bool   full;
};

// ---------------------------------------------------------------------------
// DataLogger
// ---------------------------------------------------------------------------

class DataLogger
{
public:
    DataLogger();
    ~DataLogger();

    // Mount LittleFS (if not already mounted) and restore uploadedOffset.
    bool begin(
        const char* logFile  = "/system.log",
        const char* idxFile  = "/system.idx",
        size_t      maxFileSize = 102400
    );

    // Append one log entry.  Returns false when logger is full.
    bool log(const String& timestamp, char level, const String& message);

    // True when no more entries can be stored.
    bool isFull();

    // Return the next batch of unuploaded log lines, up to maxPacketSize bytes.
    PacketData getPacket(size_t maxPacketSize);

    // Advance the uploadedOffset after a confirmed server ACK.
    bool markUploaded(size_t endOffset);

    // Erase everything and reset offset to 0.
    bool clear();

    // Snapshot of logger state (no mutex needed for reading primitives,
    // but we take it anyway for consistency).
    LoggerInfo getInfo();

    // Explicit compaction (also called internally).
    bool compact();

private:
    // Internal helpers – must be called with mutex already held
    size_t  _fileSize();
    size_t  _loadOffset();
    bool    _saveOffset(size_t offset);
    bool    _compactIfNeeded();   // decides whether compaction is due
    bool    _doCompact();         // actual compaction work

    const char*    _logFile;
    const char*    _idxFile;
    size_t         _maxFileSize;
    size_t         _uploadedOffset;

    SemaphoreHandle_t _mutex;
};