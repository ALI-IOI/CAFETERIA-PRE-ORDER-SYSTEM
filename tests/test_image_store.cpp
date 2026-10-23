#include "testing.h"
#include "storage/image_store.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static const std::string PNG_BYTES = std::string("\x89PNG\r\n\x1a\n", 8) + std::string(40, 'x');
static const std::string JPEG_BYTES = std::string("\xFF\xD8\xFF\xE0", 4) + std::string(40, 'x');
static const std::string WEBP_BYTES = std::string("RIFF\x10\x00\x00\x00WEBPVP8 ", 16) + std::string(40, 'x');

static std::string freshDataDir(const char* name) {
    fs::path p = fs::temp_directory_path() / "cafeteria_tests" / name;
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

TEST(jpeg_png_webp_detected) {
    CHECK_EQ(detectImage(PNG_BYTES), ImageKind::Png);
    CHECK_EQ(detectImage(JPEG_BYTES), ImageKind::Jpeg);
    CHECK_EQ(detectImage(WEBP_BYTES), ImageKind::Webp);
}

TEST(fake_image_refused) {
    CHECK_EQ(detectImage("<html><script>alert(1)</script></html>"), ImageKind::None);
    CHECK_EQ(detectImage(std::string("MZ\x90\x00", 4)), ImageKind::None);
    CHECK_EQ(detectImage(std::string("\x89PNG", 4)), ImageKind::None);  // truncated header
    CHECK_EQ(detectImage(""), ImageKind::None);
    std::string dir = freshDataDir("img_fake");
    std::string out, err;
    CHECK(!storeItemImage(dir, 1, "<html>", "", out, err));
    CHECK(!fs::exists(fs::path(dir) / "images") || fs::is_empty(fs::path(dir) / "images"));
}

TEST(oversized_refused) {
    std::string dir = freshDataDir("img_big");
    std::string big = PNG_BYTES + std::string(MAX_IMAGE_BYTES, 'x');
    std::string out, err;
    CHECK(!storeItemImage(dir, 1, big, "", out, err));
    CHECK(err.find("2 MB") != std::string::npos);
}

TEST(stored_name_is_server_chosen) {
    std::string dir = freshDataDir("img_store");
    std::string out, err;
    CHECK(storeItemImage(dir, 3, PNG_BYTES, "", out, err));
    CHECK(validImageFileName(out));
    CHECK(out.rfind("item-3-", 0) == 0);
    CHECK(out.size() > 4 && out.substr(out.size() - 4) == ".png");
    CHECK(fs::exists(fs::path(dir) / "images" / out));
    CHECK(storeItemImage(dir, 3, JPEG_BYTES, "", out, err));
    CHECK(out.substr(out.size() - 4) == ".jpg");
}

TEST(old_picture_deleted_on_replace) {
    std::string dir = freshDataDir("img_replace");
    std::string first, second, err;
    storeItemImage(dir, 2, PNG_BYTES, "", first, err);
    CHECK(storeItemImage(dir, 2, WEBP_BYTES, first, second, err));
    CHECK(first != second);
    CHECK(!fs::exists(fs::path(dir) / "images" / first));
    CHECK(fs::exists(fs::path(dir) / "images" / second));
}

TEST(path_like_old_name_not_deleted) {
    std::string dir = freshDataDir("img_traversal");
    { std::ofstream o(dir + "/menu.txt"); o << "keep me"; }
    std::string out, err;
    CHECK(storeItemImage(dir, 1, PNG_BYTES, "../menu.txt", out, err));
    CHECK(fs::exists(dir + "/menu.txt"));
}

TEST(valid_image_file_names) {
    CHECK(validImageFileName("item-12-0123456789abcdef.webp"));
    CHECK(!validImageFileName("../menu.txt"));
    CHECK(!validImageFileName("item-1-xyz.png"));
    CHECK(!validImageFileName("item-1-0123456789abcdef.exe"));
    CHECK(!validImageFileName("item--0123456789abcdef.png"));
    CHECK(!validImageFileName(""));
}
