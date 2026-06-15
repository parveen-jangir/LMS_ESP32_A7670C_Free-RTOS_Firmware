#include "DataLogger.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// RAII mutex guard
struct MutexGuard
{
    SemaphoreHandle_t _h;
    MutexGuard(SemaphoreHandle_t h) : _h(h) { xSemaphoreTake(_h, portMAX_DELAY); }
    ~MutexGuard()                            { xSemaphoreGive(_h); }
};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

DataLogger::DataLogger()
    : _logFile(nullptr)
    , _idxFile(nullptr)
    , _maxFileSize(102400)
    , _uploadedOffset(0)
    , _mutex(nullptr)
    , isInitialized(false)
{}

DataLogger::~DataLogger()
{
    if (_mutex) vSemaphoreDelete(_mutex);
}

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------

bool DataLogger::begin(const char* logFile, const char* idxFile, size_t maxFileSize)
{
    _logFile     = logFile;
    _idxFile     = idxFile;
    _maxFileSize = maxFileSize;

    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) return false;

    if (!LittleFS.begin(true))   // true = format on first use
        return false;

    // Restore persisted offset
    _uploadedOffset = _loadOffset();

    // Sanity-check against actual file size
    size_t sz = _fileSize();
    if (_uploadedOffset > sz)
        _uploadedOffset = 0;

    isInitialized = true;
    return true;
}

// ---------------------------------------------------------------------------
// log
// ---------------------------------------------------------------------------

bool DataLogger::log(char level, const String& message)
{
    if(isInitialized == false) return false;
    String timestamp = getFromatedTime();
    
    MutexGuard guard(_mutex);

    // Try compaction before checking fullness
    _compactIfNeeded();

    size_t sz = _fileSize();

    // Full: file at limit and nothing has been uploaded yet
    if (sz >= _maxFileSize && _uploadedOffset == 0)
        return false;

    // Still full after compaction attempt
    if (sz >= _maxFileSize)
        return false;

    File f = LittleFS.open(_logFile, "a");
    if (!f) return false;

    // Format: MMDD,HHMMSS,L,MESSAGE\n
    String entry = timestamp + "," + level + "," + message + "\n";
    size_t written = f.print(entry);
    f.close();

    return (written == entry.length());
}

// ---------------------------------------------------------------------------
// isFull
// ---------------------------------------------------------------------------

bool DataLogger::isFull()
{
    MutexGuard guard(_mutex);
    size_t sz = _fileSize();
    return (sz >= _maxFileSize && _uploadedOffset == 0);
}

// ---------------------------------------------------------------------------
// getPacket
// ---------------------------------------------------------------------------

PacketData DataLogger::getPacket(size_t maxPacketSize)
{
    MutexGuard guard(_mutex);

    PacketData pkt;
    pkt.packetSize   = 0;
    pkt.startOffset  = _uploadedOffset;
    pkt.endOffset    = _uploadedOffset;

    File f = LittleFS.open(_logFile, "r");
    if (!f) return pkt;

    size_t fileSize = f.size();
    if (_uploadedOffset >= fileSize)
    {
        f.close();
        return pkt;
    }

    f.seek(_uploadedOffset);

    // Read line by line, stop before exceeding maxPacketSize
    while (f.available())
    {
        size_t lineStart = f.position();

        // Read one line into a small char buffer to avoid loading entire file
        String line = f.readStringUntil('\n');
        if (line.length() == 0) break;

        // Account for the '\n' we consumed
        size_t lineLen = line.length() + 1;  // +1 for '\n'

        if (pkt.packetSize + lineLen > maxPacketSize)
        {
            // Don't include this line; restore position (not needed since we
            // track endOffset separately, but keeps f consistent)
            break;
        }

        pkt.data       += line + "\n";
        pkt.packetSize += lineLen;
        pkt.endOffset   = f.position();   // byte after '\n'
    }

    f.close();
    return pkt;
}

// ---------------------------------------------------------------------------
// markUploaded
// ---------------------------------------------------------------------------

bool DataLogger::markUploaded(size_t endOffset)
{
    MutexGuard guard(_mutex);

    size_t sz = _fileSize();
    if (endOffset > sz) endOffset = sz;   // clamp

    _uploadedOffset = endOffset;
    bool ok = _saveOffset(_uploadedOffset);

    // Opportunistic compaction
    _compactIfNeeded();

    return ok;
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

bool DataLogger::clear()
{
    MutexGuard guard(_mutex);

    LittleFS.remove(_logFile);
    _uploadedOffset = 0;
    return _saveOffset(0);
}

// ---------------------------------------------------------------------------
// getInfo
// ---------------------------------------------------------------------------

LoggerInfo DataLogger::getInfo()
{
    MutexGuard guard(_mutex);

    LoggerInfo info;
    info.currentSize     = _fileSize();
    info.maxSize         = _maxFileSize;
    info.uploadedOffset  = _uploadedOffset;
    info.pendingBytes    = (info.currentSize > _uploadedOffset)
                            ? (info.currentSize - _uploadedOffset)
                            : 0;
    info.full            = (info.currentSize >= _maxFileSize && _uploadedOffset == 0);
    return info;
}

// ---------------------------------------------------------------------------
// compact (public – takes mutex)
// ---------------------------------------------------------------------------

bool DataLogger::compact()
{
    MutexGuard guard(_mutex);
    return _doCompact();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

size_t DataLogger::_fileSize()
{
    if (!LittleFS.exists(_logFile)) return 0;
    File f = LittleFS.open(_logFile, "r");
    if (!f) return 0;
    size_t sz = f.size();
    f.close();
    return sz;
}

size_t DataLogger::_loadOffset()
{
    if (!LittleFS.exists(_idxFile)) return 0;

    File f = LittleFS.open(_idxFile, "r");
    if (!f) return 0;

    String s = f.readString();
    f.close();
    return (size_t)s.toInt();
}

bool DataLogger::_saveOffset(size_t offset)
{
    File f = LittleFS.open(_idxFile, "w");
    if (!f) return false;
    f.print(offset);
    f.close();
    return true;
}

bool DataLogger::_compactIfNeeded()
{
    size_t sz = _fileSize();

    bool thresholdReached = (_uploadedOffset >= _maxFileSize / 4);
    bool fileAtLimit      = (sz >= _maxFileSize);

    if ((thresholdReached || fileAtLimit) && _uploadedOffset > 0)
        return _doCompact();

    return true;
}

bool DataLogger::_doCompact()
{
    if (_uploadedOffset == 0) return true;   // nothing to remove

    const char* tmpPath = "/system.tmp";

    File src = LittleFS.open(_logFile, "r");
    if (!src) return false;

    size_t srcSize = src.size();
    if (_uploadedOffset >= srcSize)
    {
        // Everything has been uploaded – just truncate
        src.close();
        LittleFS.remove(_logFile);
        _uploadedOffset = 0;
        return _saveOffset(0);
    }

    // Seek to pending section
    src.seek(_uploadedOffset);

    File dst = LittleFS.open(tmpPath, "w");
    if (!dst)
    {
        src.close();
        return false;
    }

    // Copy pending bytes in chunks
    const size_t CHUNK = 512;
    uint8_t buf[CHUNK];

    while (src.available())
    {
        size_t toRead = src.available();
        if (toRead > CHUNK) toRead = CHUNK;
        size_t got = src.read(buf, toRead);
        if (got == 0) break;
        dst.write(buf, got);
    }

    src.close();
    dst.close();

    LittleFS.remove(_logFile);
    LittleFS.rename(tmpPath, _logFile);

    _uploadedOffset = 0;
    return _saveOffset(0);
}

String DataLogger::getFromatedTime()
{
    time_t now = time(nullptr);
    if (now < 0)
        return "0000,000000";

    time_t ist = now + (5 * 3600) + (30 * 60);

    struct tm *t = gmtime(&ist);

    struct tm timeinfo;
    localtime_r(&ist, &timeinfo);

    char buffer[16];
    snprintf(buffer, sizeof(buffer),
             "%02d%02d,%02d%02d%02d",
             timeinfo.tm_mon + 1, // Month (1-12)
             timeinfo.tm_mday,    // Day (1-31)
             timeinfo.tm_hour,    // Hour (00-23)
             timeinfo.tm_min,     // Minute (00-59)
             timeinfo.tm_sec);    // Second (00-59)

    return String(buffer);
}