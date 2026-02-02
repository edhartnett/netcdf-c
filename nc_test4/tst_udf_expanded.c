/* This is part of the netCDF package. Copyright 2005-2024 University
   Corporation for Atmospheric Research/Unidata. See COPYRIGHT file
   for conditions of use.

   Test expanded user-defined format slots (UDF0-UDF9).
   Tests the infrastructure for 10 UDF slots.

   Ed Hartnett
*/

#include "config.h"
#include <nc_tests.h>
#include "err_macros.h"
#include "netcdf.h"
#include "nc4dispatch.h"
#include "hdf5dispatch.h"
#include "netcdf_dispatch.h"

#define FILE_NAME "tst_udf_expanded.nc"

/* Simple dispatch functions for testing */
int test_open(const char *path, int mode, int basepe, size_t *chunksizehintp,
              void *parameters, const NC_Dispatch *dispatch, int ncid)
{
    return NC_NOERR;
}

int test_close(int ncid, void *v)
{
    return NC_NOERR;
}

int test_inq_format(int ncid, int *formatp)
{
    if (formatp) *formatp = NC_FORMAT_NETCDF4;
    return NC_NOERR;
}

int test_inq_format_extended(int ncid, int *formatp, int *modep)
{
    if (formatp) *formatp = NC_FORMAT_NETCDF4;
    if (modep) *modep = NC_NETCDF4;
    return NC_NOERR;
}

/* Minimal dispatch table for testing */
static NC_Dispatch test_dispatcher = {
    NC_FORMATX_UDF0,
    NC_DISPATCH_VERSION,
    NC_RO_create, test_open,
    NC_RO_redef, NC_RO__enddef, NC_RO_sync, NC_RO_sync, test_close, NC_RO_set_fill,
    test_inq_format, test_inq_format_extended,
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
    printf("\n*** Testing expanded UDF slots (UDF0-UDF9).\n");
    
    printf("*** testing all 10 UDF mode flags...");
    {
        int mode_flags[10] = {NC_UDF0, NC_UDF1, NC_UDF2, NC_UDF3, NC_UDF4,
                              NC_UDF5, NC_UDF6, NC_UDF7, NC_UDF8, NC_UDF9};
        int i;
        
        /* Verify each mode flag is unique */
        for (i = 0; i < 10; i++) {
            for (int j = i + 1; j < 10; j++) {
                if (mode_flags[i] == mode_flags[j]) {
                    printf("ERROR: UDF%d and UDF%d have same mode flag!\n", i, j);
                    ERR;
                }
            }
        }
        
        /* Verify NC_FORMAT_ALL includes all UDF flags */
        for (i = 0; i < 10; i++) {
            if (!(NC_FORMAT_ALL & mode_flags[i])) {
                printf("ERROR: NC_FORMAT_ALL missing UDF%d flag!\n", i);
                ERR;
            }
        }
    }
    SUMMARIZE_ERR;
    
    printf("*** testing all 10 UDF format constants...");
    {
        int formats[10] = {NC_FORMATX_UDF0, NC_FORMATX_UDF1, NC_FORMATX_UDF2,
                          NC_FORMATX_UDF3, NC_FORMATX_UDF4, NC_FORMATX_UDF5,
                          NC_FORMATX_UDF6, NC_FORMATX_UDF7, NC_FORMATX_UDF8,
                          NC_FORMATX_UDF9};
        int i;
        
        /* Verify each format constant is unique */
        for (i = 0; i < 10; i++) {
            for (int j = i + 1; j < 10; j++) {
                if (formats[i] == formats[j]) {
                    printf("ERROR: FORMATX_UDF%d and FORMATX_UDF%d have same value!\n", i, j);
                    ERR;
                }
            }
        }
        
        /* Verify they are in expected range */
        if (NC_FORMATX_UDF0 != 8) {
            printf("ERROR: NC_FORMATX_UDF0 should be 8, got %d\n", NC_FORMATX_UDF0);
            ERR;
        }
        if (NC_FORMATX_UDF1 != 9) {
            printf("ERROR: NC_FORMATX_UDF1 should be 9, got %d\n", NC_FORMATX_UDF1);
            ERR;
        }
    }
    SUMMARIZE_ERR;
    
    printf("*** testing NC_MAX_UDF_FORMATS constant...");
    {
        if (NC_MAX_UDF_FORMATS != 10) {
            printf("ERROR: NC_MAX_UDF_FORMATS should be 10, got %d\n", NC_MAX_UDF_FORMATS);
            ERR;
        }
    }
    SUMMARIZE_ERR;
    
    printf("*** testing simultaneous registration of all 10 UDF slots...");
    {
        int mode_flags[10] = {NC_UDF0, NC_UDF1, NC_UDF2, NC_UDF3, NC_UDF4,
                              NC_UDF5, NC_UDF6, NC_UDF7, NC_UDF8, NC_UDF9};
        char magic[10][8] = {"MAG0", "MAG1", "MAG2", "MAG3", "MAG4",
                            "MAG5", "MAG6", "MAG7", "MAG8", "MAG9"};
        NC_Dispatch *disp_in;
        char magic_in[NC_MAX_MAGIC_NUMBER_LEN + 1];
        int i;
        
        /* Register all 10 slots */
        for (i = 0; i < 10; i++) {
            if (nc_def_user_format(mode_flags[i] | NC_NETCDF4, &test_dispatcher, magic[i])) {
                printf("ERROR: Failed to register UDF%d\n", i);
                ERR;
            }
        }
        
        /* Verify all 10 slots are registered */
        for (i = 0; i < 10; i++) {
            if (nc_inq_user_format(mode_flags[i], &disp_in, magic_in)) {
                printf("ERROR: Failed to query UDF%d\n", i);
                ERR;
            }
            if (disp_in != &test_dispatcher) {
                printf("ERROR: UDF%d dispatch table mismatch\n", i);
                ERR;
            }
            if (strncmp(magic[i], magic_in, strlen(magic[i]))) {
                printf("ERROR: UDF%d magic number mismatch: expected %s, got %s\n",
                       i, magic[i], magic_in);
                ERR;
            }
        }
    }
    SUMMARIZE_ERR;
    
    printf("*** testing UDF flag bit positions don't conflict...");
    {
        /* UDF0 and UDF1 should be in lower 16 bits */
        if (NC_UDF0 >= 0x10000 || NC_UDF1 >= 0x10000) {
            printf("ERROR: UDF0/UDF1 should be in lower 16 bits\n");
            ERR;
        }
        
        /* UDF2-UDF9 should be in upper 16 bits */
        if (NC_UDF2 < 0x10000 || NC_UDF3 < 0x10000 || NC_UDF4 < 0x10000 ||
            NC_UDF5 < 0x10000 || NC_UDF6 < 0x10000 || NC_UDF7 < 0x10000 ||
            NC_UDF8 < 0x10000 || NC_UDF9 < 0x10000) {
            printf("ERROR: UDF2-UDF9 should be in upper 16 bits\n");
            ERR;
        }
        
        /* Verify no conflicts with existing mode flags */
        int existing_flags[] = {NC_NOWRITE, NC_WRITE, NC_CLOBBER, NC_NOCLOBBER,
                               NC_DISKLESS, NC_MMAP, NC_64BIT_OFFSET, NC_64BIT_DATA,
                               NC_CLASSIC_MODEL, NC_NETCDF4, NC_SHARE};
        int num_existing = sizeof(existing_flags) / sizeof(existing_flags[0]);
        int udf_flags[] = {NC_UDF0, NC_UDF1, NC_UDF2, NC_UDF3, NC_UDF4,
                          NC_UDF5, NC_UDF6, NC_UDF7, NC_UDF8, NC_UDF9};
        
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < num_existing; j++) {
                if (udf_flags[i] & existing_flags[j]) {
                    printf("ERROR: UDF%d conflicts with existing flag 0x%x\n",
                           i, existing_flags[j]);
                    ERR;
                }
            }
        }
    }
    SUMMARIZE_ERR;
    
    printf("*** testing error handling for invalid UDF indices...");
    {
        NC_Dispatch *disp_in;
        
        /* Test with invalid mode flag (not a UDF flag) */
        if (nc_inq_user_format(NC_NETCDF4, &disp_in, NULL) != NC_EINVAL) {
            printf("ERROR: Should reject non-UDF mode flag\n");
            ERR;
        }
        
        /* Test with multiple UDF flags set (invalid) */
        if (nc_def_user_format(NC_UDF0 | NC_UDF1, &test_dispatcher, NULL) != NC_EINVAL) {
            printf("ERROR: Should reject multiple UDF flags\n");
            ERR;
        }
    }
    SUMMARIZE_ERR;
    
    FINAL_RESULTS;
}
