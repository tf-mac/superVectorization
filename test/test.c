#include <stdio.h>
#include <math.h>

int test(int x) {
    int y = x * 8;
    return y;
}
int main() {
    for (int i = 0; i < 1000000000; ++i) {
        int j = test(i);
        if (j < 0) {
            j += i;
        }
        if (j < 0) {
            j += i;
        }
    }
    return test(7);
}
