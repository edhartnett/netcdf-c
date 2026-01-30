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

### Option B: Initialization Function Registration

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

# Filter plugin path (also supported)
NETCDF.PLUGIN.PATH=/usr/local/lib/netcdf/plugins:/opt/custom/plugins
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

### Phase 1: Filter Plugin Path Support (Simple)

**Files to modify:**
- `libdispatch/dplugins.c` - Modify `buildinitialpluginpath()`

**Changes:**
```c
static int buildinitialpluginpath(NCPluginList* dirs)
{
    int stat = NC_NOERR;
    const char* pluginpath = NULL;
    
    /* Priority order:
       1. HDF5_PLUGIN_PATH environment variable (highest)
       2. NETCDF.PLUGIN.PATH from RC file
       3. NETCDF_PLUGIN_SEARCH_PATH build constant (fallback)
    */
    pluginpath = getenv(PLUGIN_ENV);
    if(pluginpath == NULL) {
        pluginpath = NC_rclookup("NETCDF.PLUGIN.PATH", NULL, NULL);
    }
    if(pluginpath == NULL) {
        pluginpath = NETCDF_PLUGIN_SEARCH_PATH;
    }
    if((stat = ncaux_plugin_path_parse(pluginpath,'\0',dirs))) goto done;

done:
    return stat;
}
```

**Effort:** ~1 hour  
**Risk:** Low - simple extension of existing code

### Phase 2: UDF Plugin Loading (Complex)

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
       
       // Try UDF0
       const char* lib0 = NC_rclookup("NETCDF.UDF0.LIBRARY", NULL, NULL);
       const char* init0 = NC_rclookup("NETCDF.UDF0.INIT", NULL, NULL);
       const char* magic0 = NC_rclookup("NETCDF.UDF0.MAGIC", NULL, NULL);
       if(lib0 && init0) {
           if((stat = load_udf_plugin(0, lib0, init0, magic0))) 
               nclog(NCLOGWARN, "Failed to load UDF0 plugin: %s", lib0);
       }
       
       // Try UDF1
       const char* lib1 = NC_rclookup("NETCDF.UDF1.LIBRARY", NULL, NULL);
       const char* init1 = NC_rclookup("NETCDF.UDF1.INIT", NULL, NULL);
       const char* magic1 = NC_rclookup("NETCDF.UDF1.MAGIC", NULL, NULL);
       if(lib1 && init1) {
           if((stat = load_udf_plugin(1, lib1, init1, magic1)))
               nclog(NCLOGWARN, "Failed to load UDF1 plugin: %s", lib1);
       }
       
       return NC_NOERR; // Don't fail initialization if plugins fail
   }
   ```

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

### Phase 3: Documentation and Testing

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

## Alternatives Considered

### Option A: Direct Library Path in RC

```ini
NETCDF.UDF0.LIBRARY=/path/to/myformat.so
```

Library would need to export a known symbol name (e.g., `netcdf_udf0_dispatch_table`).

**Rejected because:**
- Less flexible (fixed symbol names)
- Harder for plugin developers
- No initialization hook for complex setup

## Timeline

- **Phase 1 (Filter paths):** 1 day
- **Phase 2 (UDF loading):** 3 days
- **Phase 3 (Documentation/Testing):** 2 days
- **Total:** ~1 week of development time

## Open Questions

1. Should we support multiple plugins per UDF slot (UDF0, UDF1)?
2. Should we add `NETCDF.UDF2`, `NETCDF.UDF3`, etc. for more formats?
3. Should plugin loading be lazy (on first use) or eager (at initialization)?
4. Should we cache loaded library handles or reload each time?
5. Do we need a plugin unload/cleanup mechanism?

## References

- Current UDF API: `include/netcdf.h` - `nc_def_user_format()`
- RC file implementation: `libdispatch/drc.c`
- Plugin path system: `libdispatch/dplugins.c`
- Dispatch tables: `include/ncdispatch.h`
