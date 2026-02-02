/* This is part of the netCDF package. Copyright 2005-2024 University
   Corporation for Atmospheric Research/Unidata. See COPYRIGHT file
   for conditions of use.

   Integration tests for UDF functionality.
   Tests end-to-end file operations with user-defined formats.

   Ed Hartnett
*/

#include "config.h"
#include <nc_tests.h>
#include "err_macros.h"
#include "netcdf.h"
#include "nc4dispatch.h"
#include "hdf5dispatch.h"
#include "netcdf_dispatch.h"

#define FILE_NAME "tst_udf_integration.nc"
#define MAGIC_NUMBER "INTTEST"

/* Track which functions were called */
static int open_called = 0;
static int close_called = 0;
static int inq_format_called = 0;

/* Test dispatch functions that track calls */
int
integration_open(const char *path, int mode, int basepe, size_t *chunksizehintp,
                 void *parameters, const NC_Dispatch *dispatch, int ncid)
{
    open_called = 1;
    return NC_NOERR;
}

int
integration_close(int ncid, void *v)
{
    close_called = 1;
    return NC_NOERR;
}

int
integration_inq_format(int ncid, int *formatp)
{
    inq_format_called = 1;
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    return NC_NOERR;
}

int
integration_inq_format_extended(int ncid, int *formatp, int *modep)
{
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    if (modep)
        *modep = NC_NETCDF4;
    return NC_NOERR;
}

/* Minimal dispatch table for integration testing */
static NC_Dispatch integration_dispatcher = {
    NC_FORMATX_UDF0,
    NC_DISPATCH_VERSION,
    NC_RO_create, integration_open,
    NC_RO_redef, NC_RO__enddef, NC_RO_sync, NC_RO_sync, integration_close, NC_RO_set_fill,
    integration_inq_format, integration_inq_format_extended,
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

int
main(int argc, char **argv)
{
    printf("\n*** Testing UDF integration.\n");
    
    printf("*** testing file operations with UDF format...");
    {
        int ncid;
        FILE *fp;
        char dummy_data[20] = "0123456789ABCDEFGHI";
        
        /* Create a file with magic number */
        if (!(fp = NCfopen(FILE_NAME, "w"))) ERR;
        if (fwrite(MAGIC_NUMBER, sizeof(char), strlen(MAGIC_NUMBER), fp)
            != strlen(MAGIC_NUMBER)) ERR;
        if (fwrite(dummy_data, sizeof(char), strlen(dummy_data), fp)
            != strlen(dummy_data)) ERR;
        if (fclose(fp)) ERR;
        
        /* Register UDF with magic number */
        if (nc_def_user_format(NC_UDF0 | NC_NETCDF4, &integration_dispatcher,
                               MAGIC_NUMBER)) ERR;
        
        /* Reset call tracking */
        open_called = 0;
        close_called = 0;
        inq_format_called = 0;
        
        /* Open file - should use UDF dispatcher based on magic number */
        if (nc_open(FILE_NAME, 0, &ncid)) ERR;
        
        /* Verify our dispatcher was used */
        if (!open_called) {
            printf("ERROR: UDF open function was not called\n");
            ERR;
        }
        
        /* Test nc_inq_format */
        int format;
        if (nc_inq_format(ncid, &format)) ERR;
        if (!inq_format_called) {
            printf("ERROR: UDF inq_format function was not called\n");
            ERR;
        }
        if (format != NC_FORMAT_NETCDF4) {
            printf("ERROR: Expected NC_FORMAT_NETCDF4, got %d\n", format);
            ERR;
        }
        
        /* Test nc_inq_format_extended */
        int format_ext, mode;
        if (nc_inq_format_extended(ncid, &format_ext, &mode)) ERR;
        if (format_ext != NC_FORMAT_NETCDF4) {
            printf("ERROR: Extended format mismatch\n");
            ERR;
        }
        
        /* Close file */
        if (nc_close(ncid)) ERR;
        if (!close_called) {
            printf("ERROR: UDF close function was not called\n");
            ERR;
        }
    }
    SUMMARIZE_ERR;
    
    printf("*** testing multiple UDF formats simultaneously...");
    {
        int ncid;
        FILE *fp;
        char magic1[] = "MAGIC1";
        char magic2[] = "MAGIC2";
        char dummy_data[20] = "0123456789ABCDEFGHI";
        
        /* Register two different UDFs */
        if (nc_def_user_format(NC_UDF1 | NC_NETCDF4, &integration_dispatcher,
                               magic1)) ERR;
        if (nc_def_user_format(NC_UDF2 | NC_NETCDF4, &integration_dispatcher,
                               magic2)) ERR;
        
        /* Create file with magic1 */
        if (!(fp = NCfopen("test_magic1.nc", "w"))) ERR;
        if (fwrite(magic1, sizeof(char), strlen(magic1), fp) != strlen(magic1)) ERR;
        if (fwrite(dummy_data, sizeof(char), strlen(dummy_data), fp)
            != strlen(dummy_data)) ERR;
        if (fclose(fp)) ERR;
        
        /* Create file with magic2 */
        if (!(fp = NCfopen("test_magic2.nc", "w"))) ERR;
        if (fwrite(magic2, sizeof(char), strlen(magic2), fp) != strlen(magic2)) ERR;
        if (fwrite(dummy_data, sizeof(char), strlen(dummy_data), fp)
            != strlen(dummy_data)) ERR;
        if (fclose(fp)) ERR;
        
        /* Open both files - each should use correct UDF */
        if (nc_open("test_magic1.nc", 0, &ncid)) ERR;
        if (nc_close(ncid)) ERR;
        
        if (nc_open("test_magic2.nc", 0, &ncid)) ERR;
        if (nc_close(ncid)) ERR;
    }
    SUMMARIZE_ERR;
    
    printf("*** testing UDF with explicit mode flag...");
    {
        int ncid;
        
        /* Create empty file */
        if (!(FILE *fp = NCfopen("test_explicit.nc", "w"))) ERR;
        if (fclose(fp)) ERR;
        
        /* Register UDF without magic number */
        if (nc_def_user_format(NC_UDF3 | NC_NETCDF4, &integration_dispatcher, NULL)) ERR;
        
        /* Open with explicit UDF mode flag */
        open_called = 0;
        if (nc_open("test_explicit.nc", NC_UDF3, &ncid)) ERR;
        if (!open_called) {
            printf("ERROR: UDF not used with explicit mode flag\n");
            ERR;
        }
        if (nc_close(ncid)) ERR;
    }
    SUMMARIZE_ERR;
    
    FINAL_RESULTS;
}
