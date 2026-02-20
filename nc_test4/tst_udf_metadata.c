/* This is part of the netCDF package. Copyright 2018 University
   Corporation for Atmospheric Research/Unidata. See COPYRIGHT file
   for conditions of use.

   Test the public UDF metadata construction API (netcdf_udf.h).

   This exercises nc_udf_file_open, nc_udf_def_dim, nc_udf_def_var,
   nc_udf_put_att, nc_udf_set_file_info, nc_udf_get_file_info,
   nc_udf_set_var_info, and nc_udf_file_close, then verifies the
   metadata is queryable through the standard NC4 dispatch functions.

   See https://github.com/Unidata/netcdf-c/issues/3277

   Ed Hartnett
*/

#include "config.h"
#include <string.h>
#include <nc_tests.h>
#include "err_macros.h"
#include "netcdf.h"
#include "netcdf_udf.h"
#include "nc4dispatch.h"
#include "netcdf_dispatch.h"

#define FILE_NAME "tst_udf_metadata.nc"

/* Test constants. */
#define LAT_NAME "latitude"
#define LON_NAME "longitude"
#define TIME_NAME "time"
#define TEMP_NAME "temperature"
#define PRES_NAME "pressure"
#define TITLE_ATT_NAME "title"
#define UNITS_ATT_NAME "units"
#define LAT_LEN 180
#define LON_LEN 360
#define TEMP_FILL -999.0f
#define TITLE_VAL "UDF metadata test"
#define UNITS_VAL "K"

/* Sentinel value for format_file_info / format_var_info round-trip. */
static int dummy_file_info = 42;
static int dummy_var_info = 99;

/* ------------------------------------------------------------------ */
/* Minimal UDF dispatch callbacks.                                    */
/* ------------------------------------------------------------------ */

/* The open callback uses the public UDF metadata API to build the
 * in-memory model, exactly as a real UDF handler would. */
static int
udf_open(const char *path, int mode, int basepe, size_t *chunksizehintp,
         void *parameters, const NC_Dispatch *dispatch, int ncid)
{
    int lat_dimid, lon_dimid, time_dimid;
    int temp_varid, pres_varid;
    int retval;
    float fill = TEMP_FILL;
    size_t chunks[2];

    /* 1. Initialize file metadata. */
    if ((retval = nc_udf_file_open(ncid, path, mode)))
        return retval;

    /* 2. Store format-specific file info. */
    if ((retval = nc_udf_set_file_info(ncid, &dummy_file_info)))
        return retval;

    /* 3. Define dimensions. */
    if ((retval = nc_udf_def_dim(ncid, LAT_NAME, LAT_LEN, &lat_dimid)))
        return retval;
    if ((retval = nc_udf_def_dim(ncid, LON_NAME, LON_LEN, &lon_dimid)))
        return retval;
    if ((retval = nc_udf_def_dim(ncid, TIME_NAME, NC_UNLIMITED, &time_dimid)))
        return retval;

    /* 4. Define a 2-D float variable (contiguous, with fill value). */
    {
        int dimids[2] = {lat_dimid, lon_dimid};
        if ((retval = nc_udf_def_var(ncid, TEMP_NAME, NC_FLOAT,
                                     2, dimids,
                                     NC_ENDIAN_NATIVE, sizeof(float),
                                     "float", &fill,
                                     1 /* contiguous */, NULL,
                                     &dummy_var_info, &temp_varid)))
            return retval;
    }

    /* 5. Define a scalar int variable (contiguous, no fill). */
    if ((retval = nc_udf_def_var(ncid, PRES_NAME, NC_INT,
                                 0, NULL,
                                 NC_ENDIAN_LITTLE, sizeof(int),
                                 "int", NULL,
                                 1 /* contiguous */, NULL,
                                 NULL, &pres_varid)))
        return retval;

    /* 6. Define a global attribute. */
    if ((retval = nc_udf_put_att(ncid, NC_GLOBAL, TITLE_ATT_NAME,
                                 NC_CHAR, strlen(TITLE_VAL),
                                 TITLE_VAL)))
        return retval;

    /* 7. Define a variable attribute on temp. */
    if ((retval = nc_udf_put_att(ncid, temp_varid, UNITS_ATT_NAME,
                                 NC_CHAR, strlen(UNITS_VAL),
                                 UNITS_VAL)))
        return retval;

    return NC_NOERR;
}

static int
udf_close(int ncid, void *v)
{
    return nc_udf_file_close(ncid);
}

static int
udf_abort(int ncid)
{
    return nc_udf_file_close(ncid);
}

static int
udf_inq_format(int ncid, int *formatp)
{
    if (formatp)
        *formatp = NC_FORMAT_NETCDF4;
    return NC_NOERR;
}

static int
udf_inq_format_extended(int ncid, int *formatp, int *modep)
{
    if (formatp)
        *formatp = NC_FORMATX_UDF0;
    if (modep)
        *modep = 0;
    return NC_NOERR;
}

static int
udf_get_vara(int ncid, int varid, const size_t *start, const size_t *count,
             void *value, nc_type t)
{
    /* Not needed for metadata tests. */
    return NC_NOERR;
}

/* Dispatch table — populated at runtime for MSVC compatibility. */
static NC_Dispatch udf_dispatcher;

static void
init_dispatcher(void)
{
    memset(&udf_dispatcher, 0, sizeof(udf_dispatcher));

    udf_dispatcher.model = NC_FORMATX_UDF0;
    udf_dispatcher.dispatch_version = NC_DISPATCH_VERSION;

    udf_dispatcher.create = NC_RO_create;
    udf_dispatcher.open = udf_open;

    udf_dispatcher.redef = NC_RO_redef;
    udf_dispatcher._enddef = NC_RO__enddef;
    udf_dispatcher.sync = NC_RO_sync;
    udf_dispatcher.abort = udf_abort;
    udf_dispatcher.close = udf_close;
    udf_dispatcher.set_fill = NC_RO_set_fill;
    udf_dispatcher.inq_format = udf_inq_format;
    udf_dispatcher.inq_format_extended = udf_inq_format_extended;

    udf_dispatcher.inq = NC4_inq;
    udf_dispatcher.inq_type = NC4_inq_type;

    udf_dispatcher.def_dim = NC_RO_def_dim;
    udf_dispatcher.inq_dimid = NC4_inq_dimid;
    udf_dispatcher.inq_dim = NC4_inq_dim;
    udf_dispatcher.inq_unlimdim = NC4_inq_unlimdim;
    udf_dispatcher.rename_dim = NC_RO_rename_dim;

    udf_dispatcher.inq_att = NC4_inq_att;
    udf_dispatcher.inq_attid = NC4_inq_attid;
    udf_dispatcher.inq_attname = NC4_inq_attname;
    udf_dispatcher.rename_att = NC_RO_rename_att;
    udf_dispatcher.del_att = NC_RO_del_att;
    udf_dispatcher.get_att = NC4_get_att;
    udf_dispatcher.put_att = NC_RO_put_att;

    udf_dispatcher.def_var = NC_RO_def_var;
    udf_dispatcher.inq_varid = NC4_inq_varid;
    udf_dispatcher.rename_var = NC_RO_rename_var;
    udf_dispatcher.get_vara = udf_get_vara;
    udf_dispatcher.put_vara = NC_RO_put_vara;
    udf_dispatcher.get_vars = NCDEFAULT_get_vars;
    udf_dispatcher.put_vars = NCDEFAULT_put_vars;
    udf_dispatcher.get_varm = NCDEFAULT_get_varm;
    udf_dispatcher.put_varm = NCDEFAULT_put_varm;

    udf_dispatcher.inq_var_all = NC4_inq_var_all;

    udf_dispatcher.var_par_access = NC_NOTNC4_var_par_access;
    udf_dispatcher.def_var_fill = NC_RO_def_var_fill;

    udf_dispatcher.show_metadata = NC4_show_metadata;
    udf_dispatcher.inq_unlimdims = NC4_inq_unlimdims;

    udf_dispatcher.inq_ncid = NC4_inq_ncid;
    udf_dispatcher.inq_grps = NC4_inq_grps;
    udf_dispatcher.inq_grpname = NC4_inq_grpname;
    udf_dispatcher.inq_grpname_full = NC4_inq_grpname_full;
    udf_dispatcher.inq_grp_parent = NC4_inq_grp_parent;
    udf_dispatcher.inq_grp_full_ncid = NC4_inq_grp_full_ncid;
    udf_dispatcher.inq_varids = NC4_inq_varids;
    udf_dispatcher.inq_dimids = NC4_inq_dimids;
    udf_dispatcher.inq_typeids = NC4_inq_typeids;
    udf_dispatcher.inq_type_equal = NC4_inq_type_equal;
    udf_dispatcher.def_grp = NC_NOTNC4_def_grp;
    udf_dispatcher.rename_grp = NC_NOTNC4_rename_grp;
    udf_dispatcher.inq_user_type = NC4_inq_user_type;
    udf_dispatcher.inq_typeid = NC4_inq_typeid;

    udf_dispatcher.def_compound = NC_NOTNC4_def_compound;
    udf_dispatcher.insert_compound = NC_NOTNC4_insert_compound;
    udf_dispatcher.insert_array_compound = NC_NOTNC4_insert_array_compound;
    udf_dispatcher.inq_compound_field = NC_NOTNC4_inq_compound_field;
    udf_dispatcher.inq_compound_fieldindex = NC_NOTNC4_inq_compound_fieldindex;
    udf_dispatcher.def_vlen = NC_NOTNC4_def_vlen;
    udf_dispatcher.put_vlen_element = NC_NOTNC4_put_vlen_element;
    udf_dispatcher.get_vlen_element = NC_NOTNC4_get_vlen_element;
    udf_dispatcher.def_enum = NC_NOTNC4_def_enum;
    udf_dispatcher.insert_enum = NC_NOTNC4_insert_enum;
    udf_dispatcher.inq_enum_member = NC_NOTNC4_inq_enum_member;
    udf_dispatcher.inq_enum_ident = NC_NOTNC4_inq_enum_ident;
    udf_dispatcher.def_opaque = NC_NOTNC4_def_opaque;
    udf_dispatcher.def_var_deflate = NC_NOTNC4_def_var_deflate;
    udf_dispatcher.def_var_fletcher32 = NC_NOTNC4_def_var_fletcher32;
    udf_dispatcher.def_var_chunking = NC_NOTNC4_def_var_chunking;
    udf_dispatcher.def_var_endian = NC_NOTNC4_def_var_endian;
    udf_dispatcher.def_var_filter = NC_NOTNC4_def_var_filter;
    udf_dispatcher.set_var_chunk_cache = NC_NOTNC4_set_var_chunk_cache;
    udf_dispatcher.get_var_chunk_cache = NC_NOTNC4_get_var_chunk_cache;
#if NC_DISPATCH_VERSION >= 3
    udf_dispatcher.inq_var_filter_ids = NC_NOOP_inq_var_filter_ids;
    udf_dispatcher.inq_var_filter_info = NC_NOOP_inq_var_filter_info;
#endif
#if NC_DISPATCH_VERSION >= 4
    udf_dispatcher.def_var_quantize = NC_NOTNC4_def_var_quantize;
    udf_dispatcher.inq_var_quantize = NC_NOTNC4_inq_var_quantize;
#endif
#if NC_DISPATCH_VERSION >= 5
    udf_dispatcher.inq_filter_avail = NC_NOOP_inq_filter_avail;
#endif
}

/* ------------------------------------------------------------------ */
/* Tests                                                              */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
    init_dispatcher();

    printf("\n*** Testing UDF metadata construction API (netcdf_udf.h).\n");

    /* -------------------------------------------------------------- */
    printf("*** testing UDF open builds correct dimensions...");
    {
        int ncid;
        int ndims, nvars, natts, unlimdimid;
        int dimid;
        char name[NC_MAX_NAME + 1];
        size_t len;

        /* Create a dummy file so nc_open has something to stat. */
        {
            int tmp;
            if (nc_create(FILE_NAME, 0, &tmp)) ERR;
            if (nc_close(tmp)) ERR;
        }

        /* Register our UDF and open. */
        if (nc_def_user_format(NC_UDF0, &udf_dispatcher, NULL)) ERR;
        if (nc_open(FILE_NAME, NC_UDF0, &ncid)) ERR;

        /* Check file-level counts. */
        if (nc_inq(ncid, &ndims, &nvars, &natts, &unlimdimid)) ERR;
        if (ndims != 3) ERR;
        if (nvars != 2) ERR;
        if (natts != 1) ERR;
        if (unlimdimid != 2) ERR; /* TIME is the 3rd dim (id 2) */

        /* Check latitude dimension. */
        if (nc_inq_dimid(ncid, LAT_NAME, &dimid)) ERR;
        if (dimid != 0) ERR;
        if (nc_inq_dim(ncid, dimid, name, &len)) ERR;
        if (strcmp(name, LAT_NAME)) ERR;
        if (len != LAT_LEN) ERR;

        /* Check longitude dimension. */
        if (nc_inq_dimid(ncid, LON_NAME, &dimid)) ERR;
        if (dimid != 1) ERR;
        if (nc_inq_dim(ncid, dimid, name, &len)) ERR;
        if (strcmp(name, LON_NAME)) ERR;
        if (len != LON_LEN) ERR;

        /* Check unlimited time dimension. */
        if (nc_inq_dimid(ncid, TIME_NAME, &dimid)) ERR;
        if (dimid != 2) ERR;
        if (nc_inq_dim(ncid, dimid, name, &len)) ERR;
        if (strcmp(name, TIME_NAME)) ERR;
        if (len != NC_UNLIMITED) ERR;

        if (nc_close(ncid)) ERR;
    }
    SUMMARIZE_ERR;

    /* -------------------------------------------------------------- */
    printf("*** testing UDF open builds correct variables...");
    {
        int ncid;
        int varid;
        char name[NC_MAX_NAME + 1];
        nc_type xtype;
        int ndims, dimids[2], natts;

        if (nc_open(FILE_NAME, NC_UDF0, &ncid)) ERR;

        /* Check temperature variable. */
        if (nc_inq_varid(ncid, TEMP_NAME, &varid)) ERR;
        if (varid != 0) ERR;
        if (nc_inq_var(ncid, varid, name, &xtype, &ndims, dimids, &natts)) ERR;
        if (strcmp(name, TEMP_NAME)) ERR;
        if (xtype != NC_FLOAT) ERR;
        if (ndims != 2) ERR;
        if (dimids[0] != 0 || dimids[1] != 1) ERR;
        if (natts != 1) ERR; /* "units" attribute */

        /* Check pressure variable. */
        if (nc_inq_varid(ncid, PRES_NAME, &varid)) ERR;
        if (varid != 1) ERR;
        if (nc_inq_var(ncid, varid, name, &xtype, &ndims, NULL, &natts)) ERR;
        if (strcmp(name, PRES_NAME)) ERR;
        if (xtype != NC_INT) ERR;
        if (ndims != 0) ERR;
        if (natts != 0) ERR;

        if (nc_close(ncid)) ERR;
    }
    SUMMARIZE_ERR;

    /* -------------------------------------------------------------- */
    printf("*** testing UDF open builds correct attributes...");
    {
        int ncid;
        int varid;
        nc_type xtype;
        size_t len;
        char buf[256];

        if (nc_open(FILE_NAME, NC_UDF0, &ncid)) ERR;

        /* Check global attribute. */
        if (nc_inq_att(ncid, NC_GLOBAL, TITLE_ATT_NAME, &xtype, &len)) ERR;
        if (xtype != NC_CHAR) ERR;
        if (len != strlen(TITLE_VAL)) ERR;
        memset(buf, 0, sizeof(buf));
        if (nc_get_att_text(ncid, NC_GLOBAL, TITLE_ATT_NAME, buf)) ERR;
        if (strncmp(buf, TITLE_VAL, len)) ERR;

        /* Check variable attribute on temperature. */
        if (nc_inq_varid(ncid, TEMP_NAME, &varid)) ERR;
        if (nc_inq_att(ncid, varid, UNITS_ATT_NAME, &xtype, &len)) ERR;
        if (xtype != NC_CHAR) ERR;
        if (len != strlen(UNITS_VAL)) ERR;
        memset(buf, 0, sizeof(buf));
        if (nc_get_att_text(ncid, varid, UNITS_ATT_NAME, buf)) ERR;
        if (strncmp(buf, UNITS_VAL, len)) ERR;

        if (nc_close(ncid)) ERR;
    }
    SUMMARIZE_ERR;

    /* -------------------------------------------------------------- */
    printf("*** testing nc_udf_set/get_file_info round-trip...");
    {
        int ncid;
        void *info_out = NULL;

        if (nc_open(FILE_NAME, NC_UDF0, &ncid)) ERR;

        /* Retrieve the format_file_info we stored in udf_open. */
        if (nc_udf_get_file_info(ncid, &info_out)) ERR;
        if (info_out != &dummy_file_info) ERR;
        if (*(int *)info_out != 42) ERR;

        /* nc_udf_get_file_info with NULL output pointer should fail. */
        if (nc_udf_get_file_info(ncid, NULL) != NC_EINVAL) ERR;

        if (nc_close(ncid)) ERR;
    }
    SUMMARIZE_ERR;

    /* -------------------------------------------------------------- */
    printf("*** testing nc_udf_set/get_var_info round-trip...");
    {
        int ncid, varid;
        int new_info = 77;
        void *info_out = NULL;

        if (nc_open(FILE_NAME, NC_UDF0, &ncid)) ERR;

        /* The temperature variable got dummy_var_info via nc_udf_def_var.
         * Verify we can read it back. */
        if (nc_inq_varid(ncid, TEMP_NAME, &varid)) ERR;
        if (nc_udf_get_var_info(ncid, varid, &info_out)) ERR;
        if (info_out != &dummy_var_info) ERR;
        if (*(int *)info_out != 99) ERR;

        /* Overwrite with nc_udf_set_var_info and read back. */
        if (nc_udf_set_var_info(ncid, varid, &new_info)) ERR;
        if (nc_udf_get_var_info(ncid, varid, &info_out)) ERR;
        if (info_out != &new_info) ERR;
        if (*(int *)info_out != 77) ERR;

        /* nc_udf_get_var_info with NULL output pointer should fail. */
        if (nc_udf_get_var_info(ncid, varid, NULL) != NC_EINVAL) ERR;

        if (nc_close(ncid)) ERR;
    }
    SUMMARIZE_ERR;

    /* -------------------------------------------------------------- */
    printf("*** testing bad ncid returns NC_EBADID...");
    {
        int dimid;
        void *info;

        /* All functions should reject a bogus ncid. */
        if (nc_udf_file_open(99999, "bogus", 0) != NC_EBADID) ERR;
        if (nc_udf_set_file_info(99999, NULL) != NC_EBADID) ERR;
        if (nc_udf_get_file_info(99999, &info) != NC_EBADID) ERR;
        if (nc_udf_file_close(99999) != NC_EBADID) ERR;
        if (nc_udf_def_dim(99999, "x", 10, &dimid) != NC_EBADID) ERR;
        if (nc_udf_put_att(99999, NC_GLOBAL, "a", NC_INT, 0, NULL) != NC_EBADID) ERR;
    }
    SUMMARIZE_ERR;

    /* -------------------------------------------------------------- */
    printf("*** testing multiple open/close cycles don't leak...");
    {
        int ncid;
        int i;

        for (i = 0; i < 10; i++)
        {
            if (nc_open(FILE_NAME, NC_UDF0, &ncid)) ERR;
            if (nc_close(ncid)) ERR;
        }
    }
    SUMMARIZE_ERR;

    FINAL_RESULTS;
}
