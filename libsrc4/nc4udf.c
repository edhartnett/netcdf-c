/* Copyright 2018, University Corporation for Atmospheric
 * Research/Unidata. See COPYRIGHT file for copying and redistribution
 * conditions. */
/**
 * @file
 * @internal Public API for User Defined Format (UDF) handlers to
 * build the netcdf-4 in-memory metadata model.
 *
 * This file implements the functions declared in netcdf_udf.h.
 * Each function is a thin wrapper around the existing internal
 * nc4internal.h functions, providing a stable public interface
 * for UDF handler authors.
 *
 * This is a PROTOTYPE implementation proposed in response to
 * https://github.com/Unidata/netcdf-c/issues/3277
 *
 * @author Edward Hartnett (Intelligent Data Design, Inc.)
 * @see include/netcdf_udf.h
 */

#include "config.h"
#include "netcdf.h"
#include "netcdf_udf.h"
#include "nc4internal.h"
#include "nc.h"

/**
 * @internal Initialize netcdf-4 metadata structures for a UDF file
 * being opened.
 *
 * @param ncid The ncid passed to the UDF open function.
 * @param path The file path.
 * @param mode The open mode flags.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @return NC_ENOMEM if out of memory.
 * @author Edward Hartnett
 */
int
nc_udf_file_open(int ncid, const char *path, int mode)
{
    NC_FILE_INFO_T *h5;
    int retval;

    if ((retval = nc4_file_list_add(ncid, path, mode, (void **)&h5)))
        return retval;

    h5->no_write = NC_TRUE;
    h5->root_grp->atts_read = 1;

    return NC_NOERR;
}

/**
 * @internal Store format-specific file info pointer.
 *
 * @param ncid The file ncid.
 * @param format_file_info Pointer to format-specific data.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @author Edward Hartnett
 */
int
nc_udf_set_file_info(int ncid, void *format_file_info)
{
    NC_FILE_INFO_T *h5;
    int retval;

    if ((retval = nc4_find_grp_h5(ncid, NULL, &h5)))
        return retval;

    h5->format_file_info = format_file_info;
    return NC_NOERR;
}

/**
 * @internal Retrieve format-specific file info pointer.
 *
 * @param ncid The file ncid.
 * @param format_file_infop Pointer that receives the format-specific data.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @author Edward Hartnett
 */
int
nc_udf_get_file_info(int ncid, void **format_file_infop)
{
    NC_FILE_INFO_T *h5;
    int retval;

    if (!format_file_infop)
        return NC_EINVAL;

    if ((retval = nc4_find_grp_h5(ncid, NULL, &h5)))
        return retval;

    *format_file_infop = h5->format_file_info;
    return NC_NOERR;
}

/**
 * @internal Free all netcdf-4 metadata for a UDF file being closed.
 *
 * @param ncid The file ncid.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @author Edward Hartnett
 */
int
nc_udf_file_close(int ncid)
{
    NC_FILE_INFO_T *h5;
    int retval;

    if ((retval = nc4_find_grp_h5(ncid, NULL, &h5)))
        return retval;

    return nc4_nc4f_list_del(h5);
}

/**
 * @internal Define a dimension in the root group of a UDF file.
 *
 * @param ncid The file ncid.
 * @param name Dimension name.
 * @param len Dimension length.
 * @param dimidp Pointer that receives the new dimension ID.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @return NC_ENOMEM if out of memory.
 * @author Edward Hartnett
 */
int
nc_udf_def_dim(int ncid, const char *name, size_t len, int *dimidp)
{
    NC_FILE_INFO_T *h5;
    NC_DIM_INFO_T *dim;
    int retval;

    if ((retval = nc4_find_grp_h5(ncid, NULL, &h5)))
        return retval;

    if ((retval = nc4_dim_list_add(h5->root_grp, name, len, -1, &dim)))
        return retval;

    if (dimidp)
        *dimidp = dim->hdr.id;

    return NC_NOERR;
}

/**
 * @internal Define a variable in the root group of a UDF file.
 *
 * This is the primary function UDF handlers use to register a variable
 * during file open. It handles type_info initialization, storage layout,
 * dimension IDs, and format-specific info in one call, replacing the
 * pattern of calling nc4_var_list_add() + nc4_set_var_type() + manual
 * field assignment used in NEP's static nc4_var_list_add_full().
 *
 * @param ncid The file ncid.
 * @param name Variable name.
 * @param xtype NetCDF data type (e.g. NC_FLOAT, NC_INT).
 * @param ndims Number of dimensions.
 * @param dimidsp Array of dimension IDs (length ndims).
 * @param endianness Byte order: NC_ENDIAN_NATIVE, NC_ENDIAN_LITTLE,
 *                   or NC_ENDIAN_BIG.
 * @param type_size Size in bytes of the data type.
 * @param type_name Human-readable type name string.
 * @param fill_value Optional fill value pointer (may be NULL).
 * @param contiguous Non-zero for contiguous storage, 0 for chunked.
 * @param chunksizes Array of chunk sizes (length ndims), or NULL.
 * @param format_var_info Format-specific variable info (may be NULL).
 * @param varidp Pointer that receives the new variable ID.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @return NC_ENOMEM if out of memory.
 * @author Edward Hartnett
 */
int
nc_udf_def_var(int ncid, const char *name, nc_type xtype,
               int ndims, const int *dimidsp,
               int endianness, size_t type_size, const char *type_name,
               const void *fill_value, int contiguous,
               const size_t *chunksizes,
               void *format_var_info, int *varidp)
{
    NC_FILE_INFO_T *h5;
    NC_VAR_INFO_T *var;
    NC_TYPE_INFO_T *type;
    int i;
    int retval;

    if ((retval = nc4_find_grp_h5(ncid, NULL, &h5)))
        return retval;

    /* Add the variable to the group's variable list. */
    if ((retval = nc4_var_list_add(h5->root_grp, name, ndims, &var)))
        return retval;

    var->created = NC_TRUE;
    var->written_to = NC_TRUE;
    var->format_var_info = format_var_info;
    var->atts_read = 1;

    /* Build the type_info struct for this variable's type.
     * nc4_var_list_add leaves type_info as NULL; we must populate it. */
    if (!(type = calloc(1, sizeof(NC_TYPE_INFO_T))))
        return NC_ENOMEM;
    if (!(type->hdr.name = strdup(type_name)))
    {
        free(type);
        return NC_ENOMEM;
    }
    type->hdr.sort = NCTYP;
    type->hdr.id = (int)xtype;
    type->size = type_size;
    type->endianness = endianness;

    /* Set the type class, matching the HDF5 layer's conventions:
     * NC_CHAR -> NC_CHAR, NC_FLOAT/NC_DOUBLE -> NC_FLOAT,
     * NC_STRING -> NC_STRING, all integer types -> NC_INT. */
    if (xtype == NC_CHAR)
        type->nc_type_class = NC_CHAR;
    else if (xtype == NC_FLOAT || xtype == NC_DOUBLE)
        type->nc_type_class = NC_FLOAT;
    else if (xtype == NC_STRING)
        type->nc_type_class = NC_STRING;
    else
        type->nc_type_class = NC_INT;

    var->type_info = type;
    var->type_info->rc++;
    var->endianness = endianness;

    /* Validate: if ndims > 0, dimidsp must not be NULL. */
    if (ndims > 0 && !dimidsp)
        return NC_EINVAL;

    /* Handle fill value. */
    if (fill_value && type_size > 0)
    {
        if (!(var->fill_value = malloc(type_size)))
        {
            retval = NC_ENOMEM;
            goto done;
        }
        memcpy(var->fill_value, fill_value, type_size);
    }

    /* Set storage layout. */
    var->storage = contiguous ? NC_CONTIGUOUS : NC_CHUNKED;

    /* Copy chunk sizes if chunked and provided. */
    if (!contiguous && chunksizes && ndims > 0)
    {
        if (!(var->chunksizes = malloc((size_t)ndims * sizeof(size_t))))
        {
            retval = NC_ENOMEM;
            goto done;
        }
        for (i = 0; i < ndims; i++)
            var->chunksizes[i] = chunksizes[i];
    }

    /* Copy dimension IDs. */
    for (i = 0; i < ndims; i++)
        var->dimids[i] = dimidsp[i];

    if (varidp)
        *varidp = var->hdr.id;

done:
    return retval;
}

/**
 * @internal Store format-specific variable info pointer.
 *
 * @param ncid The file ncid.
 * @param varid The variable ID.
 * @param format_var_info Pointer to format-specific data.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid or varid is invalid.
 * @author Edward Hartnett
 */
int
nc_udf_set_var_info(int ncid, int varid, void *format_var_info)
{
    NC_FILE_INFO_T *h5;
    NC_GRP_INFO_T *grp;
    NC_VAR_INFO_T *var;
    int retval;

    if ((retval = nc4_find_grp_h5_var(ncid, varid, &h5, &grp, &var)))
        return retval;

    var->format_var_info = format_var_info;
    return NC_NOERR;
}

/**
 * @internal Define an attribute on a variable or root group.
 *
 * @param ncid The file ncid.
 * @param varid Variable ID, or NC_GLOBAL for a global attribute.
 * @param name Attribute name.
 * @param xtype NetCDF data type.
 * @param len Number of values.
 * @param data Pointer to attribute data.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid or varid is invalid.
 * @return NC_ENOMEM if out of memory.
 * @author Edward Hartnett
 */
int
nc_udf_put_att(int ncid, int varid, const char *name,
               nc_type xtype, size_t len, const void *data)
{
    NC_FILE_INFO_T *h5;
    NC_GRP_INFO_T *grp;
    NC_ATT_INFO_T *att;
    NCindex *att_list;
    size_t type_size;
    int retval;

    if ((retval = nc4_find_grp_h5(ncid, &grp, &h5)))
        return retval;

    if (varid == NC_GLOBAL)
    {
        att_list = grp->att;

        if ((retval = nc4_att_list_add(att_list, name, &att)))
            return retval;

        /* Set container to the group so cleanup can find the file. */
        att->container = (NC_OBJ *)grp;
    }
    else
    {
        NC_VAR_INFO_T *var;
        if ((retval = nc4_find_grp_h5_var(ncid, varid, &h5, &grp, &var)))
            return retval;
        att_list = var->att;

        if ((retval = nc4_att_list_add(att_list, name, &att)))
            return retval;

        /* Set container to the variable so cleanup can find the file. */
        att->container = (NC_OBJ *)var;
    }

    att->nc_typeid = xtype;
    att->len = len;

    /* Get type size and copy data. */
    if ((retval = nc4_get_typelen_mem(h5, xtype, &type_size)))
        return retval;

    if (len > 0 && data)
    {
        if (!(att->data = malloc(len * type_size)))
            return NC_ENOMEM;
        memcpy(att->data, data, len * type_size);
    }

    att->dirty = NC_TRUE;

    return NC_NOERR;
}

/**
 * @internal Retrieve format-specific variable info pointer.
 *
 * @param ncid The file ncid.
 * @param varid The variable ID.
 * @param format_var_infop Pointer that receives the format-specific data.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid or varid is invalid.
 * @return NC_EINVAL if format_var_infop is NULL.
 * @author Edward Hartnett
 */
int
nc_udf_get_var_info(int ncid, int varid, void **format_var_infop)
{
    NC_FILE_INFO_T *h5;
    NC_GRP_INFO_T *grp;
    NC_VAR_INFO_T *var;
    int retval;

    if (!format_var_infop)
        return NC_EINVAL;

    if ((retval = nc4_find_grp_h5_var(ncid, varid, &h5, &grp, &var)))
        return retval;

    *format_var_infop = var->format_var_info;
    return NC_NOERR;
}
