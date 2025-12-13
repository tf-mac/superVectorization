#include <stdio.h>

int main() {
    int sum = 0;
    for(int i = 0; i < 10000000; ++i) {
        for(int j = 0; j < 100; ++j) {
            sum += i -j;
        }
    }
    printf("%i\n", sum);
    float div = 0;
    for(float i = 0; i < 100000; ++i) {
        for(float j = 0; j < 100; ++j) {
            div += i / j;
        }
    }
    int y = 1;
    for(int i = 0; i < 1000000000; ++i) {
        sum = y;
    }
    printf("%f\n", div);
    return 0;
}