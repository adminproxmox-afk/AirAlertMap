#include "AlertModel.h"

AlertModel::AlertModel() {
    for (int i = 0; i < RegionCount; ++i) states[i] = asUnknown;
}

void AlertModel::ResetDemo() {
    for (int i = 0; i < RegionCount; ++i) states[i] = asClear;
    const int active[] = {0, 6, 7, 9, 10, 23};
    const int unknown[] = {8, 18, 26};
    for (int i = 0; i < 6; ++i) states[active[i]] = asAlert;
    for (int i = 0; i < 3; ++i) states[unknown[i]] = asUnknown;
}

bool AlertModel::Cycle(int region) {
    if (region < 0 || region >= RegionCount) return false;
    states[region] = (states[region] + 1) % 3;
    return true;
}

int AlertModel::Count(AlertState state) const {
    if (state == asAlert) return CountActiveAlerts(states, RegionCount);
    int count = 0;
    for (int i = 0; i < RegionCount; ++i) if (states[i] == state) ++count;
    return count;
}
