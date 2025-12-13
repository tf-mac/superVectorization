
// Integer and float benchmark for Win32 and Win64
// Results are below main(), line 91

#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif
#include <time.h>

double
mygettime(void) {
# ifdef _WIN32
  struct _timeb tb;
  _ftime(&tb);
  return (double)tb.time + (0.001 * (double)tb.millitm);
# else
  struct timeval tv;
  if(gettimeofday(&tv, 0) < 0) {
    perror("oops");
  }
  return (double)tv.tv_sec + (0.000001 * (double)tv.tv_usec);
# endif
}

template< typename Type >
void my_test(const char* name) {
  volatile Type v  = 0;
  // Do not use constants or repeating values
  //  to avoid loop unroll optimizations.
  // All values >0 to avoid division by 0
  Type v0 = (Type)(rand() % 256)/16 + 1;
  Type v1 = (Type)(rand() % 256)/16 + 1;
  Type v2 = (Type)(rand() % 256)/16 + 1;
  Type v3 = (Type)(rand() % 256)/16 + 1;
  Type v4 = (Type)(rand() % 256)/16 + 1;
  Type v5 = (Type)(rand() % 256)/16 + 1;
  Type v6 = (Type)(rand() % 256)/16 + 1;
  Type v7 = (Type)(rand() % 256)/16 + 1;

  double t1 = mygettime();
  for (size_t i = 0; i < 100000000; ++i) {
    v += v0;
    v += v2;
    v += v4;
    v += v6;
  }
  printf("%s add: %f\n", name, mygettime() - t1);

  t1 = mygettime();
  for (size_t i = 0; i < 100000000; ++i) {
    v -= v1;
    v -= v3;
    v -= v5;
    v -= v7;
  }
  printf("%s sub: %f\n", name, mygettime() - t1);

  t1 = mygettime();
  for (size_t i = 0; i < 100000000; ++i) {
    v *= v0;
    v *= v2;
    v *= v4;
    v *= v6;
  }
  printf("%s mul: %f\n", name, mygettime() - t1);

  t1 = mygettime();
  for (size_t i = 0; i < 100000000; ++i) {
    v /= v1;
    v /= v3;
    v /= v5;
    v /= v7;
  }
  printf("%s div: %f\n", name, mygettime() - t1);
}

int main() {
  my_test<     short >("    short");
  my_test<       int >("      int");
  my_test<      long >("     long");
  my_test< long long >("long long");
  my_test<     float >("    float");
  my_test<    double >("   double");

  return 0;
}