#include <stdio.h>
#include "em_persistent_state.h"


EmPersistentState PS;
EmPersistentUInt16 intVal = EmPersistentUInt16(PS, "i_v", 16);
EmPersistentFloat floatVal = EmPersistentFloat(PS, "f_v", 55.3);
EmPersistentString textVal = EmPersistentString(PS, "txt", 10, "Hello!");

void setup() {
    // First way to initialize Persistent State
    EmPersistentValueList values;
    values.Append(floatVal);
    values.Append(intVal);
    values.Append(textVal);
    PS.Init(values, false);
}

void setup_() {
    // Second way to initialize Persistent State
    // NOTE: this way we cannot delete old unused values but we 
    //       spare memory by not filling a 'PersistentValueList' 
    if (PS.Init()) {
        PS.Add(floatVal);
        PS.Add(intVal);
        PS.Add(textVal);
    }
}

int main() {
    setup();

    // Storing new values to PS
    textVal = "Got new value!"; // This will be truncated because of max len of 10!
    intVal = 44;
    
    return 0;
}
