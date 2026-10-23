// Menu item pictures uploaded by staff (redesign). A file is accepted only if
// its first bytes are a JPEG, PNG or WebP signature — the name and the
// browser's claimed type are ignored — and it is saved under a name the
// server chooses, so an upload can never pick its own path.
#pragma once
#include <cstddef>
#include <string>

enum class ImageKind { None, Jpeg, Png, Webp };

const size_t MAX_IMAGE_BYTES = 2 * 1024 * 1024;  // [CONFIRM] 2 MB

ImageKind detectImage(const std::string& bytes);

// "item-<index>-<16 hex>.<jpg|png|webp>" and nothing else.
bool validImageFileName(const std::string& name);

// Writes <dataDir>/images/item-<index>-<random>.<ext> atomically and then
// deletes <dataDir>/images/<oldFile> if oldFile is a valid picture name.
// Refuses empty, oversized and non-image data.
bool storeItemImage(const std::string& dataDir, int itemIndex, const std::string& bytes, const std::string& oldFile,
                    std::string& newFile, std::string& err);
