#include <stdint.h>
#include <stdio.h>

void printInt(int32_t value) {
    printf("%d", value);
}

void printlnInt(int32_t value) {
    printf("%d\n", value);
}

int32_t getInt(void) {
    int32_t value = 0;
    if (scanf("%d", &value) != 1) {
        return 0;
    }
    return value;
}
