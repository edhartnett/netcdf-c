# RC File Support for User-Defined Format Plugins

**Author:** Ed Hartnett  
**Date:** January 30, 2026  
**Status:** Proposal

## Executive Summary

This document proposes extending the netCDF-C RC (runtime configuration) file mechanism to support automatic loading and registration of user-defined format plugins. This will allow users to configure custom format handlers through simple configuration files rather than requiring code changes.

## Background

### Current Plugin Systems

NetCDF-C currently supports two types of plugins:

1. **Filter Plugins** - HDF5/Zarr compression filters discovered via plugin paths
2. **User-Defined Format (UDF) Plugins** - Custom dispatch tables registered via `nc_def_user_format()`

Currently, UDF plugins must be registered programmatically by calling `nc_def_user_format()` before opening files. This requires:
- Modifying application code
- Recompiling applications
- No runtime flexibility for format discovery

### RC File System

The RC file system (`.ncrc`, `.dodsrc`) provides runtime configuration for:
- HTTP authentication and SSL settings
- AWS S3 credentials
- DAP protocol options
- Proxy settings

RC files are searched in this order:
1. `$NCRCENV_RC` (if set)
2. `$HOME/.ncrc`
3. `$HOME/.dodsrc`
4. `$CWD/.ncrc`
5. `$CWD/.dodsrc`

## Proposal: RC-Based Plugin Registration

### Initialization Function Registration

Allow users to specify plugin initialization functions in RC files. The library will dynamically load the plugin library and call the initialization function during library startup.

### RC File Syntax

```ini
# User-defined format plugin configuration
NETCDF.UDF0.LIBRARY=/path/to/my_format_plugin.so
NETCDF.UDF0.INIT=my_format_init

NETCDF.UDF1.LIBRARY=/path/to/another_format.so
NETCDF.UDF1.INIT=another_format_init

# Optional: specify magic number via RC
NETCDF.UDF0.MAGIC=MYFORMAT

# Dispatch table plugin search path (separate from filter plugins)
NETCDF.DISPATCH.PATH=/usr/local/lib/netcdf/dispatch:/opt/custom/dispatch
```

### Initialization Function Signature

Plugin libraries must provide an initialization function with this signature:

```c
int my_format_init(void);
```

The initialization function should:
1. Call `nc_def_user_format()` with its dispatch table
2. Return `NC_NOERR` on success or an error code on failure

Example plugin implementation:

```c
#include <netcdf.h>
#include <netcdf_dispatch.h>

// Define the dispatch table
static NC_Dispatch my_format_dispatcher = {
    NC_FORMATX_UDF0,
    NC_DISPATCH_VERSION,
    // ... function pointers ...
};

// Initialization function called by netCDF-C
int my_format_init(void) {
    return nc_def_user_format(NC_UDF0, &my_format_dispatcher, "MYFORMAT");
}
```

## Implementation Plan

### Important: Separate Plugin Path Systems

**Filter Plugins** (HDF5/Zarr compression filters):
- Use `HDF5_PLUGIN_PATH` environment variable
- Managed by existing `libdispatch/dplugins.c` code
- **Not modified by this proposal**

**Dispatch Table Plugins** (User-defined formats):
- Use `NETCDF.DISPATCH.PATH` RC file key
- New implementation for UDF plugin loading
- **Completely separate from filter plugin paths**

### Phase 1: UDF Plugin Loading

**New files to create:**
- `libdispatch/dudfplugins.c` - UDF plugin loading implementation
- `include/ncudfplugins.h` - Internal header for UDF plugin support

**Files to modify:**
- `libsrc4/nc4dispatch.c` - Call UDF plugin loader during `NC4_initialize()`
- `libdispatch/drc.c` - No changes needed (already supports arbitrary keys)

**New functions:**

```c
// Load and initialize UDF plugins from RC file
int NC_udf_load_plugins(void);

// Load a single UDF plugin
static int load_udf_plugin(int udf_number, const char* library_path, 
                          const char* init_func, const char* magic);

// Platform-specific dynamic loading
static void* load_library(const char* path);
static void* get_symbol(void* handle, const char* symbol);
static void close_library(void* handle);
```

**Implementation details:**

1. **During `NC4_initialize()`**, call `NC_udf_load_plugins()`

2. **`NC_udf_load_plugins()` logic:**
   ```c
   int NC_udf_load_plugins(void) {
       int stat = NC_NOERR;
       const char* dispatch_path = NULL;
       
       // Get optional dispatch plugin search path (separate from filter plugins)
       dispatch_path = NC_rclookup("NETCDF.DISPATCH.PATH", NULL, NULL);
       
       // Try UDF0
       const char* lib0 = NC_rclookup("NETCDF.UDF0.LIBRARY", NULL, NULL);
       const char* init0 = NC_rclookup("NETCDF.UDF0.INIT", NULL, NULL);
       const char* magic0 = NC_rclookup("NETCDF.UDF0.MAGIC", NULL, NULL);
       if(lib0 && init0) {
           if((stat = load_udf_plugin(0, lib0, init0, magic0, dispatch_path))) 
               nclog(NCLOGWARN, "Failed to load UDF0 plugin: %s", lib0);
       }
       
       // Try UDF1
       const char* lib1 = NC_rclookup("NETCDF.UDF1.LIBRARY", NULL, NULL);
       const char* init1 = NC_rclookup("NETCDF.UDF1.INIT", NULL, NULL);
       const char* magic1 = NC_rclookup("NETCDF.UDF1.MAGIC", NULL, NULL);
       if(lib1 && init1) {
           if((stat = load_udf_plugin(1, lib1, init1, magic1, dispatch_path)))
               nclog(NCLOGWARN, "Failed to load UDF1 plugin: %s", lib1);
       }
       
       return NC_NOERR; // Don't fail initialization if plugins fail
   }
   ```
   
   Note: If `NETCDF.DISPATCH.PATH` is specified, it can be used to search for libraries when relative paths are given. If not specified or if absolute paths are used in `NETCDF.UDF0.LIBRARY`, the path is used directly.

3. **Library path resolution:**
   
   The `load_udf_plugin()` function resolves library paths as follows:
   
   a. **Absolute paths** - Use directly:
      ```c
      if (is_absolute_path(library_path)) {
          full_path = strdup(library_path);
      }
      ```
   
   b. **Relative paths with NETCDF.DISPATCH.PATH** - Search each directory:
      ```c
      else if (dispatch_path != NULL) {
          // Parse dispatch_path (colon/semicolon separated)
          // Try library_path in each directory
          for each dir in dispatch_path:
              full_path = join_path(dir, library_path);
              if (file_exists(full_path)) break;
      }
      ```
   
   c. **Relative paths without NETCDF.DISPATCH.PATH** - Use system search:
      ```c
      else {
          // Let dlopen/LoadLibrary use system search paths
          full_path = strdup(library_path);
      }
      ```

4. **Platform-specific dynamic loading:**
   
   Implement wrapper functions for cross-platform compatibility:
   
   **Unix/Linux/macOS:**
   ```c
   #include <dlfcn.h>
   
   static void* load_library(const char* path) {
       void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
       if (!handle) {
           nclog(NCLOGERR, "dlopen failed: %s", dlerror());
       }
       return handle;
   }
   
   static void* get_symbol(void* handle, const char* symbol) {
       void* sym = dlsym(handle, symbol);
       if (!sym) {
           nclog(NCLOGERR, "dlsym failed for %s: %s", symbol, dlerror());
       }
       return sym;
   }
   
   static void close_library(void* handle) {
       if (handle) dlclose(handle);
   }
   ```
   
   **Windows:**
   ```c
   #include <windows.h>
   
   static void* load_library(const char* path) {
       HMODULE handle = LoadLibraryA(path);
       if (!handle) {
           DWORD err = GetLastError();
           nclog(NCLOGERR, "LoadLibrary failed: error %lu", err);
       }
       return (void*)handle;
   }
   
   static void* get_symbol(void* handle, const char* symbol) {
       void* sym = (void*)GetProcAddress((HMODULE)handle, symbol);
       if (!sym) {
           DWORD err = GetLastError();
           nclog(NCLOGERR, "GetProcAddress failed for %s: error %lu", 
                 symbol, err);
       }
       return sym;
   }
   
   static void close_library(void* handle) {
       if (handle) FreeLibrary((HMODULE)handle);
   }
   ```
   
   **Flags and options:**
   - `RTLD_NOW` - Resolve all symbols immediately (fail fast)
   - `RTLD_LOCAL` - Keep symbols local to avoid conflicts
   - Consider `RTLD_DEEPBIND` on Linux to prefer plugin's symbols

5. **Complete load_udf_plugin() implementation:**
   ```c
   static int load_udf_plugin(int udf_number, const char* library_path,
                              const char* init_func, const char* magic,
                              const char* dispatch_path)
   {
       int stat = NC_NOERR;
       void* handle = NULL;
       char* full_path = NULL;
       int (*init_function)(void) = NULL;
       int mode_flag = (udf_number == 0) ? NC_UDF0 : NC_UDF1;
       
       // Resolve library path
       if((stat = resolve_library_path(library_path, dispatch_path, &full_path)))
           goto done;
       
       // Load the library
       handle = load_library(full_path);
       if (!handle) {
           stat = NC_ENOTNC; // or new error code NC_EPLUGIN
           goto done;
       }
       
       // Get the initialization function
       init_function = (int (*)(void))get_symbol(handle, init_func);
       if (!init_function) {
           stat = NC_ENOTNC;
           goto done;
       }
       
       // Call the initialization function
       // This should call nc_def_user_format() internally
       if((stat = init_function())) {
           nclog(NCLOGERR, "Plugin init function %s failed: %d", 
                 init_func, stat);
           goto done;
       }
       
       // Verify the dispatch table was registered
       NC_Dispatch* dispatch_table = NULL;
       char magic_check[NC_MAX_MAGIC_NUMBER_LEN + 1];
       if((stat = nc_inq_user_format(mode_flag, &dispatch_table, magic_check))) {
           nclog(NCLOGERR, "Plugin did not register dispatch table");
           goto done;
       }
       
       if (dispatch_table == NULL) {
           nclog(NCLOGERR, "Plugin registered NULL dispatch table");
           stat = NC_EINVAL;
           goto done;
       }
       
       // Optionally verify magic number matches
       if (magic != NULL && strlen(magic_check) > 0) {
           if (strcmp(magic, magic_check) != 0) {
               nclog(NCLOGWARN, "Plugin magic number mismatch: "
                     "expected %s, got %s", magic, magic_check);
           }
       }
       
       nclog(NCLOGNOTE, "Successfully loaded UDF%d plugin from %s", 
             udf_number, full_path);
       
   done:
       // Note: We don't close the library handle - it stays loaded
       // The dispatch table and its functions must remain accessible
       nullfree(full_path);
       return stat;
   }
   ```

6. **Error handling and validation:**
   - **Path validation:**
     - Check library file exists and is readable
     - Validate file permissions (not world-writable)
     - Reject paths with suspicious patterns (e.g., `..`)
   
   - **Symbol validation:**
     - Verify dispatch table version matches `NC_DISPATCH_VERSION`
     - Check critical function pointers are non-NULL
     - Validate magic number format (printable ASCII, reasonable length)
   
   - **Graceful degradation:**
     - Log detailed error messages with file paths and error codes
     - Continue initialization even if plugins fail to load
     - Provide `nc_inq_user_format()` to check if UDF is available
   
   - **Security considerations:**
     - Consider adding `NETCDF.PLUGIN.VERIFY` RC key to require code signatures
     - Document security implications in user guide
     - Recommend absolute paths in production environments

**Effort:** ~2-3 days  
**Risk:** Medium - requires careful dynamic loading and error handling

### Phase 2: Documentation and Testing

**Documentation updates:**
- `docs/filters.md` - Add section on UDF plugin configuration
- `docs/pluginpath.md` - Update with RC file support
- Create `docs/udf_plugins.md` - Comprehensive UDF plugin guide
- Update `INSTALL.md` - Mention RC-based plugin configuration

**Unit Tests:**

Create comprehensive test suite in `unit_test/test_udf_plugins.c`:

1. **Test Plugin Library Creation** (`plugins/test_udf/`)
   
   Create two minimal test plugins for testing:
   
   **test_udf0_plugin.c:**
   ```c
   #include <netcdf.h>
   #include <netcdf_dispatch.h>
   
   // Minimal dispatch table for testing
   static NC_Dispatch test_udf0_dispatcher = {
       NC_FORMATX_UDF0,
       NC_DISPATCH_VERSION,
       // Minimal function pointers (can be stubs for testing)
       .create = test_udf0_create,
       .open = test_udf0_open,
       // ... other required functions ...
   };
   
   // Initialization function
   int test_udf0_init(void) {
       return nc_def_user_format(NC_UDF0, &test_udf0_dispatcher, "TESTUDF0");
   }
   ```
   
   **test_udf1_plugin.c:**
   - Similar to UDF0 but for UDF1 slot
   - Different magic number "TESTUDF1"
   - Different initialization function name

2. **RC File Parsing Tests** (`test_rc_plugin_keys`)
   
   Test that RC file keys are correctly read:
   ```c
   void test_rc_plugin_keys(void) {
       // Create temporary .ncrc file
       FILE* rc = fopen(".ncrc", "w");
       fprintf(rc, "NETCDF.UDF0.LIBRARY=/path/to/test.so\n");
       fprintf(rc, "NETCDF.UDF0.INIT=test_init\n");
       fprintf(rc, "NETCDF.UDF0.MAGIC=TESTMAGIC\n");
       fprintf(rc, "NETCDF.DISPATCH.PATH=/usr/local/lib:/opt/lib\n");
       fclose(rc);
       
       // Reinitialize RC system
       ncrc_initialize();
       
       // Verify values can be retrieved
       char* lib = NC_rclookup("NETCDF.UDF0.LIBRARY", NULL, NULL);
       assert(lib != NULL);
       assert(strcmp(lib, "/path/to/test.so") == 0);
       
       char* init = NC_rclookup("NETCDF.UDF0.INIT", NULL, NULL);
       assert(strcmp(init, "test_init") == 0);
       
       char* magic = NC_rclookup("NETCDF.UDF0.MAGIC", NULL, NULL);
       assert(strcmp(magic, "TESTMAGIC") == 0);
       
       char* path = NC_rclookup("NETCDF.DISPATCH.PATH", NULL, NULL);
       assert(strcmp(path, "/usr/local/lib:/opt/lib") == 0);
       
       // Cleanup
       unlink(".ncrc");
   }
   ```

3. **Plugin Loading Success Tests** (`test_load_plugin_success`)
   
   Test successful plugin loading:
   ```c
   void test_load_plugin_success(void) {
       // Setup RC file with test plugin
       FILE* rc = fopen(".ncrc", "w");
       fprintf(rc, "NETCDF.UDF0.LIBRARY=%s\n", TEST_PLUGIN_PATH);
       fprintf(rc, "NETCDF.UDF0.INIT=test_udf0_init\n");
       fclose(rc);
       
       // Initialize library (should load plugin)
       int stat = nc_initialize();
       assert(stat == NC_NOERR);
       
       // Verify dispatch table was registered
       NC_Dispatch* dispatch = NULL;
       char magic[NC_MAX_MAGIC_NUMBER_LEN + 1];
       stat = nc_inq_user_format(NC_UDF0, &dispatch, magic);
       assert(stat == NC_NOERR);
       assert(dispatch != NULL);
       assert(strcmp(magic, "TESTUDF0") == 0);
       
       // Cleanup
       unlink(".ncrc");
   }
   ```

4. **Path Resolution Tests** (`test_path_resolution`)
   
   Test library path resolution logic:
   ```c
   void test_path_resolution(void) {
       // Test absolute path
       char* result = NULL;
       resolve_library_path("/absolute/path/lib.so", NULL, &result);
       assert(strcmp(result, "/absolute/path/lib.so") == 0);
       free(result);
       
       // Test relative path with dispatch path
       resolve_library_path("lib.so", "/dir1:/dir2", &result);
       // Should search /dir1/lib.so, /dir2/lib.so
       
       // Test relative path without dispatch path
       resolve_library_path("lib.so", NULL, &result);
       assert(strcmp(result, "lib.so") == 0); // Pass through
       free(result);
   }
   ```

5. **Error Handling Tests** (`test_plugin_errors`)
   
   Test various error conditions:
   ```c
   void test_plugin_errors(void) {
       int stat;
       
       // Test: Library file doesn't exist
       FILE* rc = fopen(".ncrc", "w");
       fprintf(rc, "NETCDF.UDF0.LIBRARY=/nonexistent/path/lib.so\n");
       fprintf(rc, "NETCDF.UDF0.INIT=init_func\n");
       fclose(rc);
       
       stat = nc_initialize();
       // Should succeed but log warning, not fail initialization
       assert(stat == NC_NOERR);
       
       // Verify UDF0 was NOT registered
       NC_Dispatch* dispatch = NULL;
       stat = nc_inq_user_format(NC_UDF0, &dispatch, NULL);
       assert(dispatch == NULL);
       
       unlink(".ncrc");
       
       // Test: Init function doesn't exist in library
       // Test: Init function returns error
       // Test: Init function doesn't call nc_def_user_format()
       // Test: Invalid magic number format
       // ... additional error cases ...
   }
   ```

6. **Multiple Plugin Tests** (`test_multiple_plugins`)
   
   Test loading both UDF0 and UDF1:
   ```c
   void test_multiple_plugins(void) {
       FILE* rc = fopen(".ncrc", "w");
       fprintf(rc, "NETCDF.UDF0.LIBRARY=%s\n", TEST_UDF0_PATH);
       fprintf(rc, "NETCDF.UDF0.INIT=test_udf0_init\n");
       fprintf(rc, "NETCDF.UDF1.LIBRARY=%s\n", TEST_UDF1_PATH);
       fprintf(rc, "NETCDF.UDF1.INIT=test_udf1_init\n");
       fclose(rc);
       
       int stat = nc_initialize();
       assert(stat == NC_NOERR);
       
       // Verify both are registered
       NC_Dispatch* dispatch0 = NULL;
       NC_Dispatch* dispatch1 = NULL;
       nc_inq_user_format(NC_UDF0, &dispatch0, NULL);
       nc_inq_user_format(NC_UDF1, &dispatch1, NULL);
       
       assert(dispatch0 != NULL);
       assert(dispatch1 != NULL);
       assert(dispatch0 != dispatch1); // Different tables
       
       unlink(".ncrc");
   }
   ```

7. **Re-registration Tests** (`test_plugin_reregistration`)
   
   Test that re-registering overwrites previous plugin:
   ```c
   void test_plugin_reregistration(void) {
       // Register first plugin programmatically
       nc_def_user_format(NC_UDF0, &first_dispatcher, "FIRST");
       
       // Load second plugin via RC file
       FILE* rc = fopen(".ncrc", "w");
       fprintf(rc, "NETCDF.UDF0.LIBRARY=%s\n", SECOND_PLUGIN_PATH);
       fprintf(rc, "NETCDF.UDF0.INIT=second_init\n");
       fclose(rc);
       
       nc_initialize();
       
       // Verify second plugin replaced first
       NC_Dispatch* dispatch = NULL;
       char magic[NC_MAX_MAGIC_NUMBER_LEN + 1];
       nc_inq_user_format(NC_UDF0, &dispatch, magic);
       
       assert(dispatch == &second_dispatcher);
       assert(strcmp(magic, "SECOND") == 0);
       
       unlink(".ncrc");
   }
   ```

8. **Platform-Specific Tests** (`test_dynamic_loading`)
   
   Test platform-specific dynamic loading:
   ```c
   void test_dynamic_loading(void) {
       void* handle = NULL;
       void* symbol = NULL;
       
       // Load test plugin library
       handle = load_library(TEST_PLUGIN_PATH);
       assert(handle != NULL);
       
       // Get known symbol
       symbol = get_symbol(handle, "test_udf0_init");
       assert(symbol != NULL);
       
       // Try to get non-existent symbol
       symbol = get_symbol(handle, "nonexistent_function");
       assert(symbol == NULL);
       
       // Note: Don't close handle (matches production behavior)
   }
   ```

9. **Security Tests** (`test_plugin_security`)
   
   Test security validations:
   ```c
   void test_plugin_security(void) {
       // Test: Reject world-writable libraries
       // Test: Reject paths with ../
       // Test: Reject symlinks to unsafe locations
       // Test: Validate dispatch table version
       // Test: Validate function pointers non-NULL
   }
   ```

**Integration Tests:**

Create `nc_test4/tst_udf_plugin.c` for end-to-end testing:

1. **File Creation Test** - Create a file using UDF plugin
2. **File Reading Test** - Read a file created by UDF plugin
3. **Format Detection Test** - Verify magic number detection works
4. **Cross-format Test** - Mix UDF and standard NetCDF formats

**Test Infrastructure:**

- Add CMake targets for building test plugins
- Add test data directory with sample RC files
- Create shell script `unit_test/run_udf_plugin_tests.sh` for test orchestration
- Ensure tests clean up temporary files and RC files
- Add tests to CI/CD pipeline (`.github/workflows/`)

**Effort:** ~2 days  
**Risk:** Low

## Security Considerations

1. **Library Path Validation:**
   - Validate that library paths are absolute
   - Check file permissions before loading
   - Consider restricting to specific directories

2. **Symbol Validation:**
   - Verify dispatch table version matches
   - Validate function pointers are non-NULL
   - Check magic number format

3. **Error Isolation:**
   - Plugin failures should not crash the library
   - Use `try/catch` or signal handlers if available
   - Log all plugin errors for debugging

4. **Documentation:**
   - Warn users about security implications of loading arbitrary libraries
   - Recommend code signing for production plugins
   - Document best practices for plugin development

## Backwards Compatibility

- **Fully backwards compatible** - existing code continues to work
- RC files without plugin keys behave exactly as before
- Programmatic `nc_def_user_format()` still works
- No changes to existing APIs

## Benefits

1. **Runtime Configuration** - No application recompilation needed
2. **User Flexibility** - Users can add format support without code changes
3. **System Administration** - Admins can configure plugins system-wide
4. **Development** - Easier testing of new format implementations
5. **Deployment** - Simplified plugin distribution and installation

## Timeline

- **Phase 1 (UDF loading):** 3 days
- **Phase 2 (Documentation/Testing):** 2 days
- **Total:** ~1 week of development time

## Plugin Lifecycle

- **Eager loading at initialization** - Plugins are loaded during `NC4_initialize()`, before any file operations
- **No unload/cleanup required** - Plugin libraries remain loaded for the lifetime of the process
- **Re-registration allowed** - If a new plugin is registered for the same UDF constant (UDF0 or UDF1), it overwrites the previous registration
- **Library handles not cached** - The dynamic library handle is used only during initialization and not retained

Note: Lazy loading is not feasible because dispatch tables must be registered before format detection can occur during file open operations.

## Open Questions

1. Should we add `NETCDF.UDF2`, `NETCDF.UDF3`, etc. for more formats?

## References

- Current UDF API: `include/netcdf.h` - `nc_def_user_format()`
- RC file implementation: `libdispatch/drc.c`
- Plugin path system: `libdispatch/dplugins.c`
- Dispatch tables: `include/ncdispatch.h`
