#include "storage/image_store.h"
#include "auth/crypto.h"
#include "storage/file_util.h"
#include <cstdio>

static bool startsWith(const std::string& s, const char* prefix, size_t n, size_t at = 0) {
    return s.size() >= at + n && s.compare(at, n, prefix, n) == 0;
}

ImageKind detectImage(const std::string& b) {
    if (startsWith(b, "\xFF\xD8\xFF", 3)) return ImageKind::Jpeg;
    if (startsWith(b, "\x89PNG\r\n\x1a\n", 8)) return ImageKind::Png;
    if (startsWith(b, "RIFF", 4) && startsWith(b, "WEBP", 4, 8)) return ImageKind::Webp;
    return ImageKind::None;
}

static const char* extensionOf(ImageKind k) {
    switch (k) {
        case ImageKind::Jpeg: return "jpg";
        case ImageKind::Png: return "png";
        case ImageKind::Webp: return "webp";
        case ImageKind::None: break;
    }
    return "";
}

bool validImageFileName(const std::string& name) {
    // item-<digits>-<16 lower-case hex>.<jpg|png|webp>
    if (name.rfind("item-", 0) != 0) return false;
    size_t i = 5, digits = 0;
    while (i < name.size() && name[i] >= '0' && name[i] <= '9') i++, digits++;
    if (digits == 0 || digits > 3 || i >= name.size() || name[i] != '-') return false;
    i++;
    size_t hex = 0;
    while (i < name.size() && ((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'f'))) i++, hex++;
    if (hex != 16 || i >= name.size() || name[i] != '.') return false;
    std::string ext = name.substr(i + 1);
    return ext == "jpg" || ext == "png" || ext == "webp";
}

bool storeItemImage(const std::string& dataDir, int itemIndex, const std::string& bytes, const std::string& oldFile,
                    std::string& newFile, std::string& err) {
    newFile.clear();
    if (bytes.empty()) {
        err = "Choose a picture to upload.";
        return false;
    }
    if (bytes.size() > MAX_IMAGE_BYTES) {
        err = "Pictures must be 2 MB or smaller.";
        return false;
    }
    ImageKind kind = detectImage(bytes);
    if (kind == ImageKind::None) {
        err = "Only JPEG, PNG or WebP pictures can be uploaded.";
        return false;
    }
    std::string dir = dataDir + "/images";
    std::string name = "item-" + std::to_string(itemIndex) + "-" + randomToken(8) + "." + extensionOf(kind);
    std::string ioErr;
    if (!ensureDir(dir, ioErr) || !atomicWrite(dir + "/" + name, bytes, ioErr)) {
        std::fprintf(stderr, "Cannot store a picture: %s\n", ioErr.c_str());  // the path is for the log only
        err = "The picture could not be saved. Try again; if it keeps failing, tell the administrator.";
        return false;
    }
    if (validImageFileName(oldFile) && oldFile != name) std::remove((dir + "/" + oldFile).c_str());
    newFile = name;
    return true;
}
