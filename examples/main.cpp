#include <stdio.h>
#include "em_persistent_state.h"


EmPersistentState PS;
EmPersistentUInt16 intVal = EmPersistentUInt16(PS, "i_v", 16);
EmPersistentFloat floatVal = EmPersistentFloat(PS, "f_v", 55.3);
EmPersistentString strVal = EmPersistentString(PS, "txt", 10, "Hello!");

void setup() {
    EmPersistentValueList values;
    values.Append(floatVal);
    values.Append(intVal);
    values.Append(strVal);
    PS.Init(values, true);
}

int main() {
    setup();

    const char* txt = strVal;
    printf("text: %s\n", (const char*)txt);

    strVal = "Hi, this will be truncated because of max size of 10!";
    printf("text: %s\n", (const char*)strVal);

    uint16_t v1 = intVal;
    printf("i16: %d\n", (uint16_t)intVal);
    
    intVal = (float)15.8;
    int v2 = intVal;
    printf("i16: %d\n", (uint16_t)intVal);

    double v3 = floatVal;
    printf("f55.5: %g\n", (float)floatVal);

    floatVal = 123;

    return 0;
}
