#include "testing.h"
#include "ds/menu_array.h"

static MenuItem fresh(const char* name, int limit) {
    MenuItem m;
    m.category = "Fresh food";
    m.name = name;
    m.kind = ItemKind::Fresh;
    m.stock = 0;
    m.dailyLimit = limit;
    m.reservedToday = 0;
    return m;
}

TEST(menu_add_until_full_then_refuses) {
    MenuArray menu;
    menuInit(menu);
    CHECK_EQ(menu.count, 0);
    for (int i = 0; i < MAX_MENU_ITEMS; i++) {
        CHECK(menuAdd(menu, fresh("Item", 5)));
    }
    CHECK_EQ(menu.count, MAX_MENU_ITEMS);
    CHECK(!menuAdd(menu, fresh("One too many", 5)));
    CHECK_EQ(menu.count, MAX_MENU_ITEMS);
}

TEST(menu_at_out_of_range_is_null) {
    MenuArray menu;
    menuInit(menu);
    menuAdd(menu, fresh("Khichuri", 12));
    CHECK(menuAt(menu, -1) == nullptr);
    CHECK(menuAt(menu, 1) == nullptr);
    CHECK(menuAt(menu, 0) != nullptr);
    CHECK_EQ(menuAt(menu, 0)->name, std::string("Khichuri"));
}

TEST(menu_remaining_is_limit_minus_reserved) {
    MenuItem m = fresh("Tehari", 10);
    m.reservedToday = 3;
    CHECK_EQ(menuRemaining(m), 7);
    m.reservedToday = 10;
    CHECK_EQ(menuRemaining(m), 0);
}

TEST(menu_find_by_name_case_sensitive_and_missing_returns_minus1) {
    MenuArray menu;
    menuInit(menu);
    menuAdd(menu, fresh("Khichuri", 12));
    menuAdd(menu, fresh("Fried Rice", 8));
    CHECK_EQ(menuFindByName(menu, "Fried Rice"), 1);
    CHECK_EQ(menuFindByName(menu, "fried rice"), -1);
    CHECK_EQ(menuFindByName(menu, "Biryani"), -1);
}
