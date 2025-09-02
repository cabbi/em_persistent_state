#include <stdio.h>
#include "em_persistent_state.h"


EmPersistentState PS;
EmPersistentUInt16 intVal(PS, "i_v", 16);
EmPersistentFloat floatVal(PS, "f_v", 55.3);
EmPersistentString textVal(PS, "txt", 10, "Hello!");

void setup() {
    // First way to initialize Persistent State
    EmPersistentValueBase* values[] = {&floatVal, &intVal, &textVal };
    // Setting 'removeUnused' to true will remove old unused values from EEPROM handling 
    PS.begin(values, SIZE_OF(values), true);
}

void setup_() {
    // Second way to initialize Persistent State
    EmPersistentValueList values;
    // Passing pointer to 'append' since EmPersistentValue does not have copy constructor!
    values.append(&floatVal, false); 
    values.append(&intVal, false);
    values.append(&textVal, false);
    // Setting 'removeUnused' to true will remove old unused values from EEPROM handling 
    PS.begin(values, true);
}

void setup__() {
    // Third way to initialize Persistent State
    // NOTE: this way we cannot delete old unused values but we 
    //       spare memory by not filling an 'EmPersistentValueList' 
    if (PS.begin() >= 0) {
        PS.add(floatVal);
        PS.add(intVal);
        PS.add(textVal);
    }
}

bool set = false;
void loop() {
    if (!set) {
        // Storing new values to PS
        intVal = 44; // New value will be stored in EEPROM so this will be the value on next start
        textVal = "Got new value!"; // This will be truncated because of max len of 10!
        set = true;
    }
}