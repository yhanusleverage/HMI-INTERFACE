#include "Screens.h"
#include "NavShell.h"
#include "AppStrings.h"

/** Alias: raíz ≡ es SETTINGS (árbol Nuravine). */
lv_obj_t *Screens::createMenu(lv_obj_t *parent) { return Screens::createSettings(parent); }

void Screens::refreshMenu(lv_obj_t *root) { Screens::refreshSettings(root); }
