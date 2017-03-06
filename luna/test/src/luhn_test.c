#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "util/luhn.h"

int main() {
  assert( luhn("49927398716"     ));
  assert(!luhn("49927398717"     ));
  assert(!luhn("1234567812345678"));
  assert( luhn("1234567812345670")); 
  return 0;
}
