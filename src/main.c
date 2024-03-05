#include <stdio.h>
#include "defines.h"

#include "core/logger.h"
#include "core/asserts.h"

int main()
{
    printf("test");

    _ERROR("Testing beef: %i", 34);
    

    return 0;
}