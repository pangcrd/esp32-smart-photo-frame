#include "drv/sd_card_conf.h"

SDCardManager sd;
 
bool SDCardManager::begin(bool oneBitMode, uint32_t freqHz)
{   

    using namespace sdcard_io;

    Serial.println("[SD] Initializing SD_MMC...");
    SD_MMC.end();
 
    if (!SD_MMC.setPins(clk, cmd, d0, d1, d2, d3)) {
        Serial.println("[SD] setPins failed!");
        _initialized = false;
        return false;
    }
 
    if (!SD_MMC.begin("/sdcard", oneBitMode, false, freqHz)) {
        Serial.println("[SD] Mount failed!");
        _initialized = false;
        return false;
    }
 
    uint8_t type = SD_MMC.cardType();
    if (type == CARD_NONE) {
        Serial.println("[SD] No card attached");
        _initialized = false;
        return false;
    }
 
    _initialized = true;
 
    Serial.print("[SD] Card Type: ");
    if (type == CARD_MMC) Serial.println("MMC");
    else if (type == CARD_SD) Serial.println("SDSC");
    else if (type == CARD_SDHC) Serial.println("SDHC");
    else Serial.println("UNKNOWN");
 
    Serial.printf("[SD] Size: %lluMB, Used: %lluMB, Free: %lluMB\n",
                   getTotalSpace() / (1024 * 1024),
                   getUsedSpace() / (1024 * 1024),
                   getFreeSpace() / (1024 * 1024));
 
    return true;
}
 
bool SDCardManager::exists(const char *path) { return SD_MMC.exists(path); }
 
File SDCardManager::open(const char *path, const char *mode) { return SD_MMC.open(path, mode); }
 
bool SDCardManager::remove(const char *path) { return SD_MMC.remove(path); }
 
bool SDCardManager::mkdir(const char *path) { return SD_MMC.mkdir(path); }
 
bool SDCardManager::rmdir(const char *path) { return SD_MMC.rmdir(path); }
 
bool SDCardManager::rename(const char *pathFrom, const char *pathTo) { return SD_MMC.rename(pathFrom, pathTo); }
 
uint64_t SDCardManager::getTotalSpace() { return SD_MMC.totalBytes(); }
 
uint64_t SDCardManager::getUsedSpace() { return SD_MMC.usedBytes(); }
 
uint64_t SDCardManager::getFreeSpace() { return getTotalSpace() - getUsedSpace(); }
 
uint8_t SDCardManager::cardType() { return SD_MMC.cardType(); }
 
bool SDCardManager::_hasExtension(const String &filename, const char *ext)
{
    String lowerName = filename;
    String lowerExt = ext;
    lowerName.toLowerCase();
    lowerExt.toLowerCase();
 
    if (lowerName.length() < lowerExt.length()) return false;
    return lowerName.endsWith(lowerExt);
}
 
std::vector<String> SDCardManager::listFiles(const char *dir, const char *ext)
{
    std::vector<String> result;
 
    if (!_initialized) {
        Serial.println("[SD] listFiles called before begin()");
        return result;
    }
 
    File root = SD_MMC.open(dir);
    if (!root || !root.isDirectory()) {
        Serial.printf("[SD] Cannot open dir: %s\n", dir);
        return result;
    }
 
    String dirPath = dir;
    if (!dirPath.endsWith("/")) dirPath += "/";
 
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            // file.name() có thể trả về "cat.jpg" (chỉ tên) hoặc "/cat.jpg" (full path)
            // tuỳ phiên bản ESP32 core -> luôn tự chuẩn hoá lại thành absolute path.
            String name = file.name();
 
            String fullPath;
            if (name.startsWith("/")) {
                fullPath = name;
            } else {
                fullPath = dirPath + name;
            }
 
            if (ext == nullptr || _hasExtension(fullPath, ext)) {
                result.push_back(fullPath);
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close(); // QUAN TRỌNG: tránh leak file descriptor, ảnh hưởng các lần open sau
 
    return result;
}