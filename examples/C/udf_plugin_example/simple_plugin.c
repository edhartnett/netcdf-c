/* Simple UDF Plugin Example
 * This plugin can be dynamically loaded via RC file configuration.
 */

#include <netcdf.h>
#include <nc4dispatch.h>
#include <hdf5dispatch.h>
#include <stdio.h>

#define PLUGIN_MAGIC "SIMPLE"

/* Plugin dispatch functions */
static int
simple_open(const char *path, int mode, int basepe, size_t *chunksizehintp,
            void *parameters, const NC_Dispatch *dispatch, int ncid)
{
    printf("Simple plugin: Opening %s\n", path);
    return NC_NOERR;
}

static int
simple_close(int ncid, void *v)
{
    printf("Simple plugin: Closing file\n");
    return NC_NOERR;
}

static int
simple_inq_format(int ncid, int *formatp)
{
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    return NC_NOERR;
}

static int
simple_inq_format_extended(int ncid, int *formatp, int *modep)
{
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    if (modep)
        *modep = NC_NETCDF4;
    return NC_NOERR;
}

/* Dispatch table */
static NC_Dispatch simple_dispatcher = {
    NC_FORMATX_UDF0,
    NC_DISPATCH_VERSION,
    NC_RO_create, simple_open,
    NC_RO_redef, NC_RO__enddef, NC_RO_sync, NC_RO_sync, simple_close, NC_RO_set_fill,
    simple_inq_format, simple_inq_format_extended,
    NC4_inq, NC4_inq_type,
    NC_RO_def_dim, NC4_inq_dimid, HDF5_inq_dim, NC4_inq_unlimdim, NC_RO_rename_dim,
    NC4_inq_att, NC4_inq_attid, NC4_inq_attname, NC_RO_rename_att, NC_RO_del_att,
    NC4_get_att, NC_RO_put_att,
    NC_RO_def_var, NC4_inq_varid, NC_RO_rename_var,
    NC_RO_put_vara, NC_RO_put_vara, NCDEFAULT_get_vars, NCDEFAULT_put_vars,
    NCDEFAULT_get_varm, NCDEFAULT_put_varm,
    NC4_inq_var_all, NC_NOTNC4_var_par_access, NC_RO_def_var_fill,
    NC4_show_metadata, NC4_inq_unlimdims,
    NC4_inq_ncid, NC4_inq_grps, NC4_inq_grpname, NC4_inq_grpname_full,
    NC4_inq_grp_parent, NC4_inq_grp_full_ncid, NC4_inq_varids, NC4_inq_dimids,
    NC4_inq_typeids, NC4_inq_type_equal, NC_NOTNC4_def_grp, NC_NOTNC4_rename_grp,
    NC4_inq_user_type, NC4_inq_typeid,
    NC_NOTNC4_def_compound, NC_NOTNC4_insert_compound, NC_NOTNC4_insert_array_compound,
    NC_NOTNC4_inq_compound_field, NC_NOTNC4_inq_compound_fieldindex,
    NC_NOTNC4_def_vlen, NC_NOTNC4_put_vlen_element, NC_NOTNC4_get_vlen_element,
    NC_NOTNC4_def_enum, NC_NOTNC4_insert_enum, NC_NOTNC4_inq_enum_member,
    NC_NOTNC4_inq_enum_ident, NC_NOTNC4_def_opaque,
    NC_NOTNC4_def_var_deflate, NC_NOTNC4_def_var_fletcher32, NC_NOTNC4_def_var_chunking,
    NC_NOTNC4_def_var_endian, NC_NOTNC4_def_var_filter,
    NC_NOTNC4_set_var_chunk_cache, NC_NOTNC4_get_var_chunk_cache,
#if NC_DISPATCH_VERSION >= 3
    NC_NOOP_inq_var_filter_ids, NC_NOOP_inq_var_filter_info,
#endif
#if NC_DISPATCH_VERSION >= 4
    NC_NOTNC4_def_var_quantize, NC_NOTNC4_inq_var_quantize,
#endif
#if NC_DISPATCH_VERSION >= 5
    NC_NOOP_inq_filter_avail,
#endif
};

/* Initialization function - called by plugin loader */
int
simple_plugin_init(void)
{
    int ret;
    
    printf("Simple plugin: Initializing\n");
    
    ret = nc_def_user_format(NC_UDF0 | NC_NETCDF4, &simple_dispatcher, PLUGIN_MAGIC);
    if (ret != NC_NOERR) {
        fprintf(stderr, "Simple plugin: Failed to register: %s\n", nc_strerror(ret));
        return ret;
    }
    
    printf("Simple plugin: Registered successfully with magic '%s'\n", PLUGIN_MAGIC);
    return NC_NOERR;
}
