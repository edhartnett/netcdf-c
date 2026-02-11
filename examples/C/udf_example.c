/* This is part of the netCDF package. Copyright 2024 University
   Corporation for Atmospheric Research/Unidata. See COPYRIGHT file
   for conditions of use.

   Example of implementing and using a user-defined format (UDF).
   This demonstrates the basic structure needed for a UDF implementation.

   Edward Hartnett, 2/2/25
*/

#include <config.h>
#include <netcdf.h>
#include <nc4dispatch.h>
#include <hdf5dispatch.h>
#include <stdio.h>
#include <string.h>

#define FILE_NAME "udf_example.dat"
#define MAGIC_NUMBER "EXAMPLE"

/* Example dispatch function implementations */
static int
example_open(const char *path, int mode, int basepe, size_t *chunksizehintp,
             void *parameters, const NC_Dispatch *dispatch, int ncid)
{
    printf("Example UDF: Opening file %s\n", path);
    return NC_NOERR;
}

static int
example_close(int ncid, void *v)
{
    printf("Example UDF: Closing file\n");
    return NC_NOERR;
}

static int
example_inq_format(int ncid, int *formatp)
{
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    return NC_NOERR;
}

static int
example_inq_format_extended(int ncid, int *formatp, int *modep)
{
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    if (modep)
        *modep = NC_NETCDF4;
    return NC_NOERR;
}

/* Minimal dispatch table for the example format */
static NC_Dispatch example_dispatcher = {
    NC_FORMATX_UDF0,
    NC_DISPATCH_VERSION,
    
    /* Create/Open/Close functions */
    NC_RO_create,
    example_open,
    NC_RO_redef,
    NC_RO__enddef,
    NC_RO_sync,
    NC_RO_sync,
    example_close,
    NC_RO_set_fill,
    example_inq_format,
    example_inq_format_extended,
    
    /* Inquiry functions - use NC4 defaults */
    NC4_inq,
    NC4_inq_type,
    
    /* Dimension functions */
    NC_RO_def_dim,
    NC4_inq_dimid,
    HDF5_inq_dim,
    NC4_inq_unlimdim,
    NC_RO_rename_dim,
    
    /* Attribute functions */
    NC4_inq_att,
    NC4_inq_attid,
    NC4_inq_attname,
    NC_RO_rename_att,
    NC_RO_del_att,
    NC4_get_att,
    NC_RO_put_att,
    
    /* Variable functions */
    NC_RO_def_var,
    NC4_inq_varid,
    NC_RO_rename_var,
    NC_RO_put_vara,
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
    
    /* Group functions */
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
    
    /* Type functions */
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
    
    /* Advanced features */
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

int
main(int argc, char **argv)
{
    int ncid, ret;
    FILE *fp;
    char dummy_data[20] = "Example file data";
    
    printf("\n*** NetCDF User-Defined Format Example\n\n");
    
    /* Step 1: Register the user-defined format */
    printf("Step 1: Registering UDF with magic number '%s'\n", MAGIC_NUMBER);
    ret = nc_def_user_format(NC_UDF0 | NC_NETCDF4, &example_dispatcher, MAGIC_NUMBER);
    if (ret != NC_NOERR) {
        fprintf(stderr, "Error registering UDF: %s\n", nc_strerror(ret));
        return 1;
    }
    printf("  UDF registered successfully in slot UDF0\n\n");
    
    /* Step 2: Create a file with the magic number */
    printf("Step 2: Creating file with magic number\n");
    fp = fopen(FILE_NAME, "wb");
    if (!fp) {
        fprintf(stderr, "Error creating file\n");
        return 1;
    }
    fwrite(MAGIC_NUMBER, 1, strlen(MAGIC_NUMBER), fp);
    fwrite(dummy_data, 1, strlen(dummy_data), fp);
    fclose(fp);
    printf("  File created: %s\n\n", FILE_NAME);
    
    /* Step 3: Open the file - format auto-detected via magic number */
    printf("Step 3: Opening file (auto-detection via magic number)\n");
    ret = nc_open(FILE_NAME, 0, &ncid);
    if (ret != NC_NOERR) {
        fprintf(stderr, "Error opening file: %s\n", nc_strerror(ret));
        return 1;
    }
    printf("  File opened successfully (ncid=%d)\n\n", ncid);
    
    /* Step 4: Query the format */
    printf("Step 4: Querying file format\n");
    int format;
    ret = nc_inq_format(ncid, &format);
    if (ret != NC_NOERR) {
        fprintf(stderr, "Error querying format: %s\n", nc_strerror(ret));
        return 1;
    }
    printf("  Format: %d (NC_FORMAT_NETCDF4=%d)\n\n", format, NC_FORMAT_NETCDF4);
    
    /* Step 5: Close the file */
    printf("Step 5: Closing file\n");
    ret = nc_close(ncid);
    if (ret != NC_NOERR) {
        fprintf(stderr, "Error closing file: %s\n", nc_strerror(ret));
        return 1;
    }
    printf("  File closed successfully\n\n");
    
    /* Step 6: Open with explicit mode flag */
    printf("Step 6: Opening file with explicit UDF0 mode flag\n");
    ret = nc_open(FILE_NAME, NC_UDF0, &ncid);
    if (ret != NC_NOERR) {
        fprintf(stderr, "Error opening file: %s\n", nc_strerror(ret));
        return 1;
    }
    printf("  File opened successfully\n");
    nc_close(ncid);
    printf("\n");
    
    printf("*** Example completed successfully!\n");
    printf("*** See docs/user_defined_formats.md for more information\n\n");
    
    return 0;
}
