#include "../src/AlertModel.h"
#include <stdio.h>
#include <stdlib.h>

static void check(bool condition, const char* message) {
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

int main() {
    unsigned char mixed[] = {0, 1, 2, 2, 0, 1, 2, 255};
    check(CountActiveAlerts(mixed, 8) == 3, "ASM counts only active states");
    check(CountActiveAlerts(mixed, 3) == 1, "ASM respects array length");
    check(CountActiveAlerts(0, 0) == 0, "empty input");
    check(CountActiveAlerts(0, 5) == 0, "null input");
    check(CountActiveAlerts(mixed, -1) == 0, "negative count");
    unsigned char many[4096];
    for (int i = 0; i < 4096; ++i) many[i] = (i % 4 == 0) ? 2 : 0;
    check(CountActiveAlerts(many, 4096) == 1024, "ASM handles a large array");

    AlertModel model;
    check(model.Count(asUnknown) == 27, "initial data is unknown");
    check(!model.Cycle(-1) && !model.Cycle(27), "invalid region rejected");
    check(model.Cycle(0) && model.states[0] == asClear, "unknown to clear");
    check(model.Cycle(0) && model.states[0] == asAlert, "clear to alert");
    check(model.Cycle(0) && model.states[0] == asUnknown, "alert to unknown");
    model.ResetDemo();
    check(CountActiveAlerts(model.states, 27) == 6, "demo has six active regions");
    check(model.Count(asUnknown) == 3 && model.Count(asClear) == 18,
          "unknown and clear remain distinct");
    model.Cycle(0);
    model.ResetDemo();
    check(model.states[0] == asAlert, "reset restores demo state");
    puts("PASS: 14 model and real x86 assembly checks");
    return 0;
}
