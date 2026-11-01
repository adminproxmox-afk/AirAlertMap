#ifndef ALERT_MODEL_H
#define ALERT_MODEL_H

enum AlertState { asUnknown = 0, asClear = 1, asAlert = 2 };
const int RegionCount = 27;

// cdecl ABI: byte array, signed element count, result in EAX.
extern "C" int __cdecl CountActiveAlerts(const unsigned char* states, int count);

class AlertModel {
public:
    unsigned char states[RegionCount];
    AlertModel();
    void ResetDemo();
    bool Cycle(int region);
    int Count(AlertState state) const;
};
#endif
