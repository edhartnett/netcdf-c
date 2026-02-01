# RC Plugin Support with UDF Expansion (2→10 Slots)

Integrate RC file support for user-defined format plugins while expanding UDF slots from 2 to 10, enabling runtime plugin configuration for up to 10 custom formats.

## Overview

This combined proposal addresses two related needs:
1. **UDF Expansion**: Increase user-defined format slots from 2 (UDF0, UDF1) to 10 (UDF0-UDF9)
2. **RC Plugin Loading**: Enable automatic plugin loading via RC configuration files

These features work together: expanding UDF slots makes RC-based plugin loading more valuable by supporting more custom formats.

## Phase 0: Expand UDF Slots (2→10)

**Prerequisite for RC plugin loading to support 10 formats**

### Current Hardcoded Implementation

The existing system has exactly 2 UDF slots hardcoded throughout:
- Individual globals: `UDF0_dispatch_table`, `UDF1_dispatch_table`
- Individual mode flags: `NC_UDF0 = 0x0040`, `NC_UDF1 = 0x0080`
- Individual format constants: `NC_FORMATX_UDF0 = 8`, `NC_FORMATX_UDF1 = 9`
- Hardcoded if/else chains in 5+ locations

### Refactoring to Array-Based Architecture

Replace individual variables with arrays indexed 0-9:

```c
#define NC_MAX_UDF_FORMATS 10

// Global arrays (libdispatch/dfile.c)
NC_Dispatch* UDF_dispatch_tables[NC_MAX_UDF_FORMATS] = {NULL};
char UDF_magic_numbers[NC_MAX_UDF_FORMATS][NC_MAX_MAGIC_NUMBER_LEN + 1] = {""};
```

### Mode Flag Allocation

Use individual bit flags (Option A):
- `NC_UDF0 = 0x0040` (bit 6) - existing, unchanged
- `NC_UDF1 = 0x0080` (bit 7) - existing, unchanged
- `NC_UDF2 = 0x10000` (bit 16) - new, upper 16 bits
- `NC_UDF3 = 0x20000` (bit 17)
- `NC_UDF4 = 0x40000` (bit 18)
- `NC_UDF5 = 0x80000` (bit 19)
- `NC_UDF6 = 0x100000` (bit 20)
- `NC_UDF7 = 0x200000` (bit 21)
- `NC_UDF8 = 0x400000` (bit 22)
- `NC_UDF9 = 0x800000` (bit 23)

**Rationale**: Bits 8-15 are already occupied by `NC_CLASSIC_MODEL`, `NC_64BIT_OFFSET`, `NC_NETCDF4`, etc. Using upper 16 bits avoids conflicts.

### Format Constants

Continue sequential numbering:
- `NC_FORMATX_UDF0 = 8` (existing)
- `NC_FORMATX_UDF1 = 9` (existing)
- `NC_FORMATX_UDF2 = 11` through `NC_FORMATX_UDF9 = 17`

### Files to Modify (Phase 0)

1. **`include/netcdf.h`**:
   - Add `NC_UDF2` through `NC_UDF9` mode flags
   - Add `NC_FORMATX_UDF2` through `NC_FORMATX_UDF9` constants
   - Update `NC_FORMAT_ALL` mask
   - Add `NC_MAX_UDF_FORMATS` constant

2. **`include/ncdispatch.h`**:
   - Replace individual `extern` declarations with array declarations

3. **`include/nc.h`**:
   - Update `FILEINFOFLAGS` macro to include all 10 UDF formats

4. **`libdispatch/dfile.c`**:
   - Replace global variables with arrays
   - Add helper functions:
     ```c
     static int udf_mode_to_index(int mode_flag);
     static int udf_formatx_to_index(int formatx);
     ```
   - Refactor `nc_def_user_format()` to use array indexing
   - Refactor `nc_inq_user_format()` to use array indexing
   - Replace switch cases in `NC_get_dispatch()` (2 locations) with loop
   - Replace individual build checks with loop

5. **`libdispatch/dinfermodel.c`**:
   - Replace hardcoded magic number checks with loop
   - Replace hardcoded mode flag checks with loop
   - Add string mappings for "udf2" through "udf9"
   - Update `FORMATCREATE[]` array

6. **`nc_test4/tst_udf.c`**:
   - Expand `NUM_UDFS` from 2 to 10
   - Update test arrays to include all 10 slots

7. **`docs/dispatch.md`**:
   - Add UDF2-UDF9 rows to dispatch table documentation

### Helper Functions (Phase 0)

```c
// Convert NC_UDFn mode flag to index (0-9)
static int udf_mode_to_index(int mode_flag) {
    if (fIsSet(mode_flag, NC_UDF0)) return 0;
    if (fIsSet(mode_flag, NC_UDF1)) return 1;
    // Check upper 16 bits for UDF2-9
    for (int i = 2; i < NC_MAX_UDF_FORMATS; i++) {
        if (fIsSet(mode_flag, NC_UDF2 << (i - 2)))
            return i;
    }
    return -1;
}

// Convert NC_FORMATX_UDFn to index (0-9)
static int udf_formatx_to_index(int formatx) {
    if (formatx >= NC_FORMATX_UDF0 && formatx <= NC_FORMATX_UDF1)
        return formatx - NC_FORMATX_UDF0;
    if (formatx >= NC_FORMATX_UDF2 && formatx <= NC_FORMATX_UDF9)
        return formatx - NC_FORMATX_UDF2 + 2;
    return -1;
}
```

**Effort**: 2-3 days  
**Risk**: Medium - requires careful refactoring across multiple files

---

## Phase 1: RC-Based Plugin Loading

**Now supports all 10 UDF slots**

### RC File Syntax

```ini
# User-defined format plugin configuration (supports UDF0-UDF9)
NETCDF.UDF0.LIBRARY=/path/to/my_format_plugin.so
NETCDF.UDF0.INIT=my_format_init
NETCDF.UDF0.MAGIC=MYFORMAT

NETCDF.UDF1.LIBRARY=/path/to/another_format.so
NETCDF.UDF1.INIT=another_format_init

# ... up to UDF9
NETCDF.UDF9.LIBRARY=/path/to/tenth_format.so
NETCDF.UDF9.INIT=tenth_format_init
```

### Plugin Loader Implementation

**New files**:
- `libdispatch/dudfplugins.c` - UDF plugin loading implementation
- `include/ncudfplugins.h` - Internal header

**Modified files**:
- `libsrc4/nc4dispatch.c` - Call `NC_udf_load_plugins()` during `NC4_initialize()`

**Core function** (loop-based for all 10 slots):
```c
int NC_udf_load_plugins(void) {
    int stat = NC_NOERR;
    
    // Loop through all 10 UDF slots
    for (int i = 0; i < NC_MAX_UDF_FORMATS; i++) {
        char key_lib[64], key_init[64], key_magic[64];
        snprintf(key_lib, sizeof(key_lib), "NETCDF.UDF%d.LIBRARY", i);
        snprintf(key_init, sizeof(key_init), "NETCDF.UDF%d.INIT", i);
        snprintf(key_magic, sizeof(key_magic), "NETCDF.UDF%d.MAGIC", i);
        
        const char* lib = NC_rclookup(key_lib, NULL, NULL);
        const char* init = NC_rclookup(key_init, NULL, NULL);
        const char* magic = NC_rclookup(key_magic, NULL, NULL);
        
        if (lib && init) {
            if ((stat = load_udf_plugin(i, lib, init, magic)))
                nclog(NCLOGWARN, "Failed to load UDF%d plugin: %s", i, lib);
        } else if (lib || init) {
            nclog(NCLOGWARN, "Ignoring partial UDF%d configuration", i);
        }
    }
    
    return NC_NOERR; // Don't fail initialization if plugins fail
}
```

### Plugin Initialization Function

Plugin libraries must provide:
```c
int my_format_init(void) {
    return nc_def_user_format(NC_UDF0, &my_format_dispatcher, "MYFORMAT");
}
```

### Dynamic Loading (Platform-Specific)

**Unix/Linux/macOS**:
```c
#include <dlfcn.h>

static void* load_library(const char* path) {
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) nclog(NCLOGERR, "dlopen failed: %s", dlerror());
    return handle;
}

static void* get_symbol(void* handle, const char* symbol) {
    void* sym = dlsym(handle, symbol);
    if (!sym) nclog(NCLOGERR, "dlsym failed for %s: %s", symbol, dlerror());
    return sym;
}
```

**Windows**:
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
        nclog(NCLOGERR, "GetProcAddress failed for %s: error %lu", symbol, err);
    }
    return sym;
}
```

### Complete Plugin Loader

```c
static int load_udf_plugin(int udf_number, const char* library_path,
                           const char* init_func, const char* magic)
{
    int stat = NC_NOERR;
    void* handle = NULL;
    int (*init_function)(void) = NULL;
    
    // Determine mode flag from UDF number
    int mode_flag;
    if (udf_number == 0) mode_flag = NC_UDF0;
    else if (udf_number == 1) mode_flag = NC_UDF1;
    else mode_flag = NC_UDF2 << (udf_number - 2);
    
    // Load library
    handle = load_library(library_path);
    if (!handle) {
        stat = NC_ENOTNC;
        goto done;
    }
    
    // Get initialization function
    init_function = (int (*)(void))get_symbol(handle, init_func);
    if (!init_function) {
        stat = NC_ENOTNC;
        goto done;
    }
    
    // Call initialization function
    if ((stat = init_function())) {
        nclog(NCLOGERR, "Plugin init function %s failed: %d", init_func, stat);
        goto done;
    }
    
    // Verify dispatch table was registered
    NC_Dispatch* dispatch_table = NULL;
    char magic_check[NC_MAX_MAGIC_NUMBER_LEN + 1];
    if ((stat = nc_inq_user_format(mode_flag, &dispatch_table, magic_check))) {
        nclog(NCLOGERR, "Plugin did not register dispatch table");
        goto done;
    }
    
    if (dispatch_table == NULL) {
        nclog(NCLOGERR, "Plugin registered NULL dispatch table");
        stat = NC_EINVAL;
        goto done;
    }
    
    // Verify dispatch ABI version
    if (dispatch_table->version != NC_DISPATCH_VERSION) {
        nclog(NCLOGERR, "Plugin dispatch ABI mismatch: expected %d, got %d",
              NC_DISPATCH_VERSION, dispatch_table->version);
        stat = NC_EINVAL;
        goto done;
    }
    
    // Optionally verify magic number
    if (magic != NULL && strlen(magic_check) > 0) {
        if (strcmp(magic, magic_check) != 0) {
            nclog(NCLOGWARN, "Plugin magic number mismatch: expected %s, got %s",
                  magic, magic_check);
        }
    }
    
    nclog(NCLOGNOTE, "Successfully loaded UDF%d plugin from %s", 
          udf_number, library_path);
    
done:
    // Handles are intentionally not closed; the OS will reclaim at process exit.
    // The dispatch table and its functions must remain accessible.
    return stat;
}
```

### Error Handling

- **Partial configuration**: Warn if only LIBRARY or only INIT is specified
- **Missing library**: Log warning, continue initialization
- **Missing symbol**: Log error, continue initialization
- **Init failure**: Log error, continue initialization
- **ABI mismatch**: Log error, reject plugin
- **Path validation**: Check file exists, readable, not world-writable

**Effort**: 3 days  
**Risk**: Medium - dynamic loading complexity

---

## Phase 2: Testing

### Unit Tests (`unit_test/test_udf_plugins.c`)

1. **RC file parsing** - Verify all 10 UDF slots can be configured
2. **Plugin loading** - Test successful loading for multiple slots
3. **Error handling** - Missing files, bad symbols, partial config
4. **Multiple plugins** - Load 3-5 plugins simultaneously
5. **Re-registration** - Verify overwrite behavior
6. **Platform-specific** - Test dynamic loading on Unix/Windows
7. **Security** - Validate path checks, permission checks

### Integration Tests (`nc_test4/tst_udf_plugin.c`)

1. **File creation** - Create files using UDF plugins
2. **File reading** - Read files created by UDF plugins
3. **Format detection** - Verify magic number detection
4. **Mixed formats** - Use UDF and standard formats together

### Test Plugins

Create minimal test plugins for UDF0-UDF2:
- `plugins/test_udf/test_udf0_plugin.c`
- `plugins/test_udf/test_udf1_plugin.c`
- `plugins/test_udf/test_udf2_plugin.c`

Each with unique magic numbers and minimal dispatch tables.

**Effort**: 2 days  
**Risk**: Low

---

## Phase 3: Documentation

### User Documentation

- **`docs/udf_plugins.md`** (new) - Comprehensive UDF plugin guide
  - How to write a plugin
  - RC file configuration
  - All 10 UDF slots documented
  - Security best practices
  
- **`docs/filters.md`** - Add section on UDF vs filter plugins
- **`docs/pluginpath.md`** - Update with RC file support
- **`INSTALL.md`** - Mention RC-based plugin configuration

### Developer Documentation

- **`docs/dispatch.md`** - Add UDF2-UDF9 to dispatch table list
- Update architecture skill with UDF expansion details

**Effort**: 1 day  
**Risk**: Low

---

## Timeline

- **Phase 0 (UDF expansion):** 2-3 days
- **Phase 1 (RC plugin loading):** 3 days
- **Phase 2 (Testing):** 2 days
- **Phase 3 (Documentation):** 1 day
- **Total:** ~8-9 days

## Benefits

1. **10 custom formats** instead of 2
2. **Runtime configuration** - no recompilation needed
3. **Simplified deployment** - RC files instead of code changes
4. **System-wide configuration** - admins can configure plugins
5. **Backward compatible** - existing UDF0/UDF1 code unchanged

## Security Considerations

1. **Library path validation** - absolute paths, file permissions
2. **Symbol validation** - ABI version, non-NULL pointers
3. **Error isolation** - plugin failures don't crash library
4. **Documentation** - warn about loading arbitrary libraries

## Backward Compatibility

- ✅ Existing `NC_UDF0` and `NC_UDF1` unchanged
- ✅ Existing `nc_def_user_format()` API unchanged
- ✅ Existing programmatic registration still works
- ✅ RC files without plugin keys behave as before

## Success Criteria

- [ ] All 10 UDF slots can be registered programmatically
- [ ] All 10 UDF slots can be loaded via RC files
- [ ] Magic number detection works for all 10 slots
- [ ] Existing UDF0/UDF1 code continues to work
- [ ] All tests pass
- [ ] Documentation complete
