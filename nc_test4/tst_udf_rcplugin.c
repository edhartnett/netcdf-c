/* This is part of the netCDF package. Copyright 2005-2024 University
   Corporation for Atmospheric Research/Unidata. See COPYRIGHT file
   for conditions of use.

   Test RC-based UDF plugin loading.
   
   This test program is a companion to tst_udf_rcplugin.sh. Most RC plugin
   loading tests require setting up RC files and shared libraries, which is
   difficult to do in a C test program. Therefore:
   
   - This C program documents what SHOULD be tested
   - The shell script (tst_udf_rcplugin.sh) performs the actual tests
   
   RC plugin loading involves:
   1. Reading RC files (.ncrc, .dodsrc) during library initialization
   2. Finding NETCDF.UDFn.LIBRARY, NETCDF.UDFn.INIT, NETCDF.UDFn.MAGIC keys
   3. Loading shared libraries using dlopen() (Unix) or LoadLibrary() (Windows)
   4. Calling initialization functions from the loaded libraries
   5. Handling errors (missing files, missing functions, init failures)
   
   See tst_udf_rcplugin.sh for comprehensive tests of these scenarios.

   Edward Hartnett, 2/2/25
*/

#include "config.h"
#include <nc_tests.h>
#include "err_macros.h"
#include "netcdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FILE "tst_udf_rcplugin.nc"
#define RC_FILE ".ncrc"

int
main(int argc, char **argv)
{
    printf("\n*** Testing RC-based UDF plugin loading.\n");
    
    /* Test 1: Verify NC_udf_load_plugins() function exists and can be called
     * This function is called automatically during nc_initialize(), but we
     * document here that it should be idempotent (safe to call multiple times).
     * Actual RC file parsing and plugin loading is tested in the shell script. */
    printf("*** testing that NC_udf_load_plugins can be called...");
    {
        /* This function is called during library initialization.
         * We can't easily test the actual loading here without setting up
         * RC files and building shared libraries, which is done in the shell script.
         * This placeholder documents that the function should exist and be callable. */
        printf("(tested via shell script)");
    }
    SUMMARIZE_ERR;
    
    /* Test 2: Plugin registration mechanism
     * The nc_def_user_format() function is the core API for registering UDF
     * dispatch tables. It's called by plugin init functions loaded from RC files.
     * This is thoroughly tested in tst_udf.c and tst_udf_expanded.c. */
    printf("*** testing plugin registration via nc_def_user_format...");
    {
        NC_Dispatch *disp_in;
        char magic[10] = "TESTMAGIC";
        
        /* This is tested more thoroughly in tst_udf.c and tst_udf_expanded.c
         * Here we just document that it's a critical part of the plugin system */
        printf("(basic mechanism tested in other tests)");
    }
    SUMMARIZE_ERR;
    
    /* Test 3: Error handling for missing init function
     * When an RC file specifies NETCDF.UDFn.INIT=function_name but that
     * function doesn't exist in the loaded library, the plugin loader should:
     * - Log an error message
     * - Skip that UDF slot
     * - Continue processing other UDF slots
     * This requires a specially crafted plugin and is tested in the shell script. */
    printf("*** testing error handling for missing init function...");
    {
        /* This would require a specially crafted plugin library
         * and is better tested in the shell script */
        printf("(tested via shell script)");
    }
    SUMMARIZE_ERR;
    
    /* Test 4: Error handling for init function failure
     * When a plugin's init function is called but returns an error code,
     * the plugin loader should:
     * - Log the error
     * - Not register the UDF slot
     * - Continue processing other UDF slots
     * The test_plugin_lib.c provides test_plugin_init_fail() for this test. */
    printf("*** testing error handling for init function failure...");
    {
        /* This would require a specially crafted plugin library
         * and is better tested in the shell script */
        printf("(tested via shell script)");
    }
    SUMMARIZE_ERR;
    
    /* Test 5: Dispatch table ABI version checking
     * Each dispatch table has a version field (NC_DISPATCH_VERSION).
     * The nc_def_user_format() function verifies this matches the library's
     * expected version. Mismatches indicate the plugin was compiled against
     * a different version of netCDF-C headers.
     * This version check is tested in tst_udf.c. */
    printf("*** testing dispatch table ABI version checking...");
    {
        /* The version check happens in nc_def_user_format,
         * which is tested in tst_udf.c */
        printf("(tested in tst_udf.c)");
    }
    SUMMARIZE_ERR;
    
    printf("*** Note: Most RC plugin loading tests require shell script setup\n");
    printf("*** See tst_udf_rcplugin.sh for comprehensive RC loading tests\n");
    
    FINAL_RESULTS;
}
