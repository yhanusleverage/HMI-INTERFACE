#ifndef RULES_ENGINE_H
#define RULES_ENGINE_H

#include <stddef.h>

namespace RulesEngine {
void begin();
void tick();
/** Dispara acción de la regla sin evaluar condición (UI Test). */
void fire(size_t ruleIndex);
}

#endif
