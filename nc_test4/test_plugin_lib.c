/* This is part of the netCDF package. Copyright 2005-2024 University
   Corporation for Atmospheric Research/Unidata. See COPYRIGHT file
   for conditions of use.

   Test plugin library for UDF RC loading tests.
   
   This is a minimal plugin library that can be dynamically loaded to test
   the RC-based plugin loading mechanism. It provides:
   
   1. A minimal dispatch table with stub implementations
   2. Multiple initialization functions to test different scenarios:
      - test_plugin_init() - Normal initialization for UDF0
      - test_plugin_init_udf2() - Initialization for UDF2 slot
      - test_plugin_init_fail() - Intentionally failing init for error testing
   3. A magic number ("TSTPLG") for automatic format detection
   
   This library is compiled as a shared library (.so/.dylib/.dll) and loaded
   dynamically by the netCDF library when configured via RC files.

   Edward Hartnett, 2/2/25
*/

#include "config.h"
#include <nc_tests.h>
#include "netcdf.h"
#include "nc4dispatch.h"
#include "hdf5dispatch.h"
#include "netcdf_dispatch.h"

/* Magic number that identifies files handled by this plugin */
#define TEST_PLUGIN_MAGIC "TSTPLG"

/* Minimal dispatch function implementations 
 * These are stub functions that do minimal work - just enough to test
 * that the plugin loading mechanism works correctly. */
static int
test_plugin_open(const char *path, int mode, int basepe, size_t *chunksizehintp,
                 void *parameters, const NC_Dispatch *dispatch, int ncid)
{
    return NC_NOERR;
}

static int
test_plugin_abort(int ncid)
{
    return NC_NOERR;
}

static int
test_plugin_close(int ncid, void *v)
{
    return NC_NOERR;
}

static int
test_plugin_inq_format(int ncid, int *formatp)
{
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    return NC_NOERR;
}

static int
test_plugin_inq_format_extended(int ncid, int *formatp, int *modep)
{
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    if (modep)
        *modep = NC_NETCDF4;
    return NC_NOERR;
}

static int
test_plugin_get_vara(int ncid, int varid, const size_t *start, const size_t *count,
                     void *value, nc_type t)
{
    return NC_NOERR;
}

/* Dispatch table for test plugin - uses UDF0 format
 * 
 * This table maps netCDF API calls to implementation functions.
 * Most functions use read-only stubs (NC_RO_*) or NC4/HDF5 defaults
 * since this is just a test plugin. A real plugin would implement
 * custom versions of these functions to handle its specific format.
 */
static NC_Dispatch test_plugin_dispatcher = {
    NC_FORMATX_UDF0,
    NC_DISPATCH_VERSION,

    NC_RO_create,
    test_plugin_open,

    NC_RO_redef,
    NC_RO__enddef,
    NC_RO_sync,
    test_plugin_abort,
    test_plugin_close,
    NC_RO_set_fill,
    test_plugin_inq_format,
    test_plugin_inq_format_extended,

    NC4_inq,
    NC4_inq_type,

    NC_RO_def_dim,
    NC4_inq_dimid,
    HDF5_inq_dim,
    NC4_inq_unlimdim,
    NC_RO_rename_dim,

    NC4_inq_att,
    NC4_inq_attid,
    NC4_inq_attname,
    NC_RO_rename_att,
    NC_RO_del_att,
    NC4_get_att,
    NC_RO_put_att,

    NC_RO_def_var,
    NC4_inq_varid,
    NC_RO_rename_var,
    test_plugin_get_vara,
    NC_RO_put_vara,
    NCDEFAULT_get_vars,
    NCDEFAULT_put_vars,
    NCDEFAULT_get_varm,
    NCDEFAULT_put_varm,

    NC4_inq_var_all,

    NC_NOTNC4_var_par_access,
    NC_RO_def_var_fill,

    NC4_show_metadata,
    NC4_inq_unlimdims,

    NC4_inq_ncid,
    NC4_inq_grps,
    NC4_inq_grpname,
    NC4_inq_grpname_full,
    NC4_inq_grp_parent,
    NC4_inq_grp_full_ncid,
    NC4_inq_varids,
    NC4_inq_dimids,
    NC4_inq_typeids,
    NC4_inq_type_equal,
    NC_NOTNC4_def_grp,
    NC_NOTNC4_rename_grp,
    NC4_inq_user_type,
    NC4_inq_typeid,

    NC_NOTNC4_def_compound,
    NC_NOTNC4_insert_compound,
    NC_NOTNC4_insert_array_compound,
    NC_NOTNC4_inq_compound_field,
    NC_NOTNC4_inq_compound_fieldindex,
    NC_NOTNC4_def_vlen,
    NC_NOTNC4_put_vlen_element,
    NC_NOTNC4_get_vlen_element,
    NC_NOTNC4_def_enum,
    NC_NOTNC4_insert_enum,
    NC_NOTNC4_inq_enum_member,
    NC_NOTNC4_inq_enum_ident,
    NC_NOTNC4_def_opaque,
    NC_NOTNC4_def_var_deflate,
    NC_NOTNC4_def_var_fletcher32,
    NC_NOTNC4_def_var_chunking,
    NC_NOTNC4_def_var_endian,
    NC_NOTNC4_def_var_filter,
    NC_NOTNC4_set_var_chunk_cache,
    NC_NOTNC4_get_var_chunk_cache,
#if NC_DISPATCH_VERSION >= 3
    NC_NOOP_inq_var_filter_ids,
    NC_NOOP_inq_var_filter_info,
#endif
#if NC_DISPATCH_VERSION >= 4
    NC_NOTNC4_def_var_quantize,
    NC_NOTNC4_inq_var_quantize,
#endif
#if NC_DISPATCH_VERSION >= 5
    NC_NOOP_inq_filter_avail,
#endif
};

/* Initialization function called by plugin loader
 * 
 * This function is called when the plugin is loaded via RC file configuration.
 * The RC file specifies this function name in the NETCDF.UDF0.INIT key.
 * 
 * It registers the dispatch table with the netCDF library, associating it with:
 * - The UDF0 slot (NC_UDF0 mode flag)
 * - The magic number "TSTPLG" for automatic format detection
 * 
 * Returns NC_NOERR on success, error code on failure.
 */
int
test_plugin_init(void)
{
    int ret;
    
    /* Register the dispatch table with magic number.
     * After this, files starting with "TSTPLG" will be handled by this plugin. */
    if ((ret = nc_def_user_format(NC_UDF0 | NC_NETCDF4, &test_plugin_dispatcher, 
                                   TEST_PLUGIN_MAGIC)))
        return ret;
    
    return NC_NOERR;
}

/* Alternative init function for testing different UDF slots
 * 
 * This function tests that plugins can be loaded into different UDF slots.
 * It registers the same dispatch table in the UDF2 slot instead of UDF0,
 * with a different magic number ("TSTPL2").
 * 
 * This allows testing:
 * - Multiple UDF slots can be used simultaneously
 * - Different magic numbers can be registered for different slots
 * - RC file can specify different init functions for different slots
 */
int
test_plugin_init_udf2(void)
{
    int ret;
    
    /* Register in UDF2 slot with different magic number */
    if ((ret = nc_def_user_format(NC_UDF2 | NC_NETCDF4, &test_plugin_dispatcher, 
                                   "TSTPL2")))
        return ret;
    
    return NC_NOERR;
}

/* Init function that intentionally fails for error testing
 * 
 * This function is used to test error handling in the plugin loading code.
 * It returns NC_EINVAL to simulate a plugin initialization failure.
 * 
 * Tests should verify that:
 * - The error is properly detected and reported
 * - The plugin is not registered when init fails
 * - The library continues to function normally after the failure
 */
int
test_plugin_init_fail(void)
{
    /* Intentionally return error to test error handling */
    return NC_EINVAL;
}
