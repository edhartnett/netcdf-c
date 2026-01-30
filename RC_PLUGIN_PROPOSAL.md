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

**Test cases:**
- Unit test: RC file parsing for plugin keys
- Integration test: Load a test UDF plugin via RC file
- Error handling test: Invalid library paths, missing symbols
- Multi-platform test: Verify on Linux, macOS, Windows

**Example plugin for testing:**
Create a simple test plugin in `plugins/test_udf/` that:
- Implements a minimal dispatch table
- Provides an initialization function
- Can be loaded via RC file

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
