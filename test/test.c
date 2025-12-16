#include <stdio.h>
#include <math.h>

int test(int x) {
    int y = x * 8;
    return y;
}
int main() {
    for (int i = 0; i < 1000; ++i) {
        int j = test(i);
        if (j % 16 != 0) {
            printf("Error: %d is not multiple of 16\n", j);
        }
    }
    return test(7);
}
