#include <string.h>
#include <assert.h>

int luhn(const char* cc) {
  const int m[] = {0,2,4,6,8,1,3,5,7,9}; // mapping for rule 3
  int i, odd = 1, sum = 0;
 
  for (i = strlen(cc); i--; odd = !odd) {
    int digit = cc[i] - '0';
    assert(digit >= 0 && digit <= 9);
    sum += odd ? digit : m[digit];
  }
 
  return sum % 10 == 0;
}
