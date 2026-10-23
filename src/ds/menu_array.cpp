#include "ds/menu_array.h"

void menuInit(MenuArray& menu) {
    menu.count = 0;
}

bool menuAdd(MenuArray& menu, const MenuItem& item) {
    if (menu.count >= MAX_MENU_ITEMS) return false;
    menu.items[menu.count] = item;
    menu.count++;
    return true;
}

MenuItem* menuAt(MenuArray& menu, int index) {
    if (index < 0 || index >= menu.count) return nullptr;
    return &menu.items[index];
}

const MenuItem* menuAt(const MenuArray& menu, int index) {
    if (index < 0 || index >= menu.count) return nullptr;
    return &menu.items[index];
}

int menuFindByName(const MenuArray& menu, const std::string& name) {
    for (int i = 0; i < menu.count; i++) {
        if (menu.items[i].name == name) return i;
    }
    return -1;
}

int menuRemaining(const MenuItem& item) {
    return item.dailyLimit - item.reservedToday;
}
