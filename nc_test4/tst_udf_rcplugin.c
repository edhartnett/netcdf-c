/* This is part of the netCDF package. Copyright 2005-2024 University
   Corporation for Atmospheric Research/Unidata. See COPYRIGHT file
   for conditions of use.

   Test RC-based UDF plugin loading.
   
   RC plugin loading tests require setting up RC files and shared libraries,
   which is difficult to do in a C test program. All actual tests are performed
   in the companion shell script tst_udf_rcplugin.sh.

   Edward Hartnett, 2/2/25
*/

#include "config.h"
#include <nc_tests.h>
#include "err_macros.h"
#include <stdio.h>

int
main(int argc, char **argv)
{
    printf("\n*** Testing RC-based UDF plugin loading.\n");
    printf("*** All tests are performed in tst_udf_rcplugin.sh\n");
    FINAL_RESULTS;
}
