/* This is part of the netCDF package. Copyright 2005-2024 University
   Corporation for Atmospheric Research/Unidata. See COPYRIGHT file
   for conditions of use.

   Test plugin library for UDF RC loading tests.
   This is a minimal plugin that can be dynamically loaded to test
   the RC-based plugin loading mechanism.

   Ed Hartnett
*/

#include "config.h"
#include <nc_tests.h>
#include "netcdf.h"
#include "nc4dispatch.h"
#include "hdf5dispatch.h"
#include "netcdf_dispatch.h"

#define TEST_PLUGIN_MAGIC "TSTPLG"

/* Minimal dispatch function implementations */
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

/* Dispatch table for test plugin - uses UDF0 format */
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

/* Initialization function called by plugin loader */
int
test_plugin_init(void)
{
    int ret;
    
    /* Register the dispatch table with magic number */
    if ((ret = nc_def_user_format(NC_UDF0 | NC_NETCDF4, &test_plugin_dispatcher, 
                                   TEST_PLUGIN_MAGIC)))
        return ret;
    
    return NC_NOERR;
}

/* Alternative init function for testing different UDF slots */
int
test_plugin_init_udf2(void)
{
    int ret;
    
    /* Register in UDF2 slot */
    if ((ret = nc_def_user_format(NC_UDF2 | NC_NETCDF4, &test_plugin_dispatcher, 
                                   "TSTPL2")))
        return ret;
    
    return NC_NOERR;
}

/* Init function that intentionally fails for error testing */
int
test_plugin_init_fail(void)
{
    return NC_EINVAL;
}
