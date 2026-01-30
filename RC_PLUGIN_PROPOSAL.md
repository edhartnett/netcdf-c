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

3. **Platform-specific dynamic loading:**
   - Use `dlopen()`/`dlsym()`/`dlclose()` on Unix/Linux
   - Use `LoadLibrary()`/`GetProcAddress()`/`FreeLibrary()` on Windows
   - Use existing netCDF-C platform abstraction if available

4. **Error handling:**
   - Log warnings for plugin load failures
   - Don't fail library initialization if plugins fail
   - Provide clear error messages about missing symbols or libraries

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
