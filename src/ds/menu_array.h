// Menu and stock: a fixed-size array of menu-item records (proposal section 7).
// Packaged items use `stock`; fresh items use `dailyLimit` and `reservedToday`.
#pragma once
#include <string>

enum class ItemKind { Packaged, Fresh };

struct MenuItem {
    std::string category;  // e.g. "Chips", "Soft drinks", "Fresh food"
    std::string name;
    ItemKind kind;
    int stock;          // packaged items: units in stock (view-only)
    int dailyLimit;     // fresh items: portions the cafeteria will cook today
    int reservedToday;  // fresh items: portions already reserved today
    int priceTaka = 0;      // shown only; paid in cash at the counter (no online payment)
    std::string imageFile;  // server-chosen picture file in data/images, or "" for the demo picture
    bool available = true;  // false: hidden from the menu, cannot be reserved
    bool removed = false;   // retired by staff: hidden everywhere, but keeps its index (reservations refer to it)
};

const int MAX_MENU_ITEMS = 32;

struct MenuArray {
    MenuItem items[MAX_MENU_ITEMS];
    int count;
};

void menuInit(MenuArray& menu);

// Adds at index `count`. O(1). Returns false when the array is full.
bool menuAdd(MenuArray& menu, const MenuItem& item);

// O(1) access by index. Returns nullptr when the index is out of range.
MenuItem* menuAt(MenuArray& menu, int index);
const MenuItem* menuAt(const MenuArray& menu, int index);

// Linear search by exact (case-sensitive) name. Returns the index or -1.
int menuFindByName(const MenuArray& menu, const std::string& name);

// Fresh items: portions still available today.
int menuRemaining(const MenuItem& item);
