/* Test program for Simple UDF Plugin
 * This program demonstrates both automatic loading (via RC file)
 * and programmatic registration of the plugin.
 *
 * Edward Hartnett, 2/2/25
 */

#include <netcdf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FILE "simple_test.dat"
#define MAGIC "SIMPLE"

/* External init function from plugin (for programmatic loading) */
extern int simple_plugin_init(void);

int main(int argc, char **argv)
{
    int ncid, ret;
    FILE *fp;
    char data[] = "Test data for simple plugin";
    int use_rc = 0;
    
    printf("\n*** Simple UDF Plugin Test\n\n");
    
    /* Check command line argument */
    if (argc > 1 && strcmp(argv[1], "--rc") == 0) {
        use_rc = 1;
        printf("Mode: Automatic loading via RC file\n");
        printf("  Ensure ~/.ncrc contains:\n");
        printf("    NETCDF.UDF0.LIBRARY=/path/to/libsimpleplugin.so\n");
        printf("    NETCDF.UDF0.INIT=simple_plugin_init\n");
        printf("    NETCDF.UDF0.MAGIC=SIMPLE\n\n");
    } else {
        printf("Mode: Programmatic registration\n");
        printf("  (Use --rc flag to test RC file loading)\n\n");
        
        /* Register plugin programmatically */
        printf("Calling simple_plugin_init()...\n");
        ret = simple_plugin_init();
        if (ret != NC_NOERR) {
            fprintf(stderr, "Failed to initialize plugin: %s\n", nc_strerror(ret));
            return 1;
        }
        printf("\n");
    }
    
    /* Create test file with magic number */
    printf("Creating test file with magic number...\n");
    fp = fopen(TEST_FILE, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to create test file\n");
        return 1;
    }
    fwrite(MAGIC, 1, strlen(MAGIC), fp);
    fwrite(data, 1, strlen(data), fp);
    fclose(fp);
    printf("  Created: %s\n\n", TEST_FILE);
    
    /* Test 1: Open with auto-detection */
    printf("Test 1: Opening file (auto-detection via magic number)\n");
    ret = nc_open(TEST_FILE, 0, &ncid);
    if (ret != NC_NOERR) {
        fprintf(stderr, "  FAILED: %s\n", nc_strerror(ret));
        return 1;
    }
    printf("  SUCCESS: File opened (ncid=%d)\n", ncid);
    
    /* Query format */
    int format;
    nc_inq_format(ncid, &format);
    printf("  Format: %d\n", format);
    
    nc_close(ncid);
    printf("\n");
    
    /* Test 2: Open with explicit mode flag */
    printf("Test 2: Opening file with explicit NC_UDF0 flag\n");
    ret = nc_open(TEST_FILE, NC_UDF0, &ncid);
    if (ret != NC_NOERR) {
        fprintf(stderr, "  FAILED: %s\n", nc_strerror(ret));
        return 1;
    }
    printf("  SUCCESS: File opened (ncid=%d)\n", ncid);
    nc_close(ncid);
    printf("\n");
    
    /* Cleanup */
    remove(TEST_FILE);
    
    printf("*** All tests passed!\n\n");
    
    if (!use_rc) {
        printf("To test RC file loading, run:\n");
        printf("  1. Copy example.ncrc to ~/.ncrc\n");
        printf("  2. Edit paths in ~/.ncrc\n");
        printf("  3. Run: %s --rc\n\n", argv[0]);
    }
    
    return 0;
}
