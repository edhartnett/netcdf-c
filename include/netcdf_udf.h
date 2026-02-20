/*! \file netcdf_udf.h
 *
 * Public API for User Defined Format (UDF) handlers to build the
 * netcdf-4 in-memory metadata model.
 *
 * Copyright 2018, University Corporation for Atmospheric
 * Research/Unidata. See COPYRIGHT file for more info.
 *
 * \section udf_metadata_api UDF Metadata Construction API
 *
 * NetCDF UDF handlers (registered via nc_def_user_format()) need to
 * populate the netcdf-4 in-memory metadata model when opening a file,
 * so that standard NetCDF API calls work transparently on the
 * foreign-format file.
 *
 * Previously this required copying private internal headers
 * (nc4internal.h, nc.h, etc.) into the UDF handler's source tree.
 * This header provides a stable public API that replaces that need.
 *
 * \section udf_usage Usage
 *
 * In your UDF open function:
 * \code
 * #include <netcdf.h>
 * #include <netcdf_udf.h>
 *
 * int MY_UDF_open(const char *path, int mode, int basepe,
 *                 size_t *chunksizehintp, void *parameters,
 *                 const NC_Dispatch *dispatch, int ncid)
 * {
 *     int ret;
 *     // Initialize netcdf-4 metadata structures for this file
 *     if ((ret = nc_udf_file_open(ncid, path, mode)))
 *         return ret;
 *     // Define dimensions, variables, attributes...
 *     int dimid;
 *     if ((ret = nc_udf_def_dim(ncid, "x", 1024, &dimid)))
 *         return ret;
 *     return NC_NOERR;
 * }
 * \endcode
 *
 * \note This header is a PROTOTYPE proposed in response to
 *       https://github.com/Unidata/netcdf-c/issues/3277
 *       It is not yet part of the official netcdf-c release.
 *       Implementation is in libsrc4/nc4udf.c.
 *
 * \author Edward Hartnett (Intelligent Data Design, Inc.)
 */

#ifndef NETCDF_UDF_H
#define NETCDF_UDF_H

#include "netcdf.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * File lifecycle functions.
 */

/**
 * Initialize netcdf-4 metadata structures for a UDF file being opened.
 *
 * Must be called from the UDF open function before defining any
 * dimensions, variables, or attributes. Replaces the need to call
 * nc4_file_list_add() and NC_check_id() directly.
 *
 * @param ncid The ncid passed to the UDF open function.
 * @param path The file path.
 * @param mode The open mode flags.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @return NC_ENOMEM if out of memory.
 */
EXTERNL int nc_udf_file_open(int ncid, const char *path, int mode);

/**
 * Store format-specific file info pointer in the NC_FILE_INFO_T.
 *
 * Replaces direct assignment to h5->format_file_info.
 *
 * @param ncid The file ncid.
 * @param format_file_info Pointer to format-specific data (caller owns).
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 */
EXTERNL int nc_udf_set_file_info(int ncid, void *format_file_info);

/**
 * Retrieve format-specific file info pointer from NC_FILE_INFO_T.
 *
 * Replaces direct access to h5->format_file_info.
 *
 * @param ncid The file ncid.
 * @param format_file_infop Pointer that receives the format-specific data.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 */
EXTERNL int nc_udf_get_file_info(int ncid, void **format_file_infop);

/**
 * Free all netcdf-4 metadata for a UDF file being closed.
 *
 * Must be called from the UDF close/abort function. Replaces the need
 * to call nc4_nc4f_list_del() directly.
 *
 * @param ncid The file ncid.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 */
EXTERNL int nc_udf_file_close(int ncid);

/*
 * Metadata construction functions.
 * All operate on the root group of the file identified by ncid.
 */

/**
 * Define a dimension in the root group of a UDF file.
 *
 * Replaces the need to call nc4_dim_list_add() directly.
 *
 * @param ncid The file ncid.
 * @param name Dimension name.
 * @param len Dimension length (use NC_UNLIMITED for unlimited).
 * @param dimidp Pointer that receives the new dimension ID.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @return NC_ENOMEM if out of memory.
 */
EXTERNL int nc_udf_def_dim(int ncid, const char *name, size_t len,
                            int *dimidp);

/**
 * Define a variable in the root group of a UDF file.
 *
 * Replaces the pattern of calling nc4_var_list_add() + nc4_set_var_type()
 * + manual NC_VAR_INFO_T field assignment used in UDF handler open functions.
 * Handles type_info initialization, storage layout, dimension IDs, fill
 * values, and format-specific info in a single call.
 *
 * @param ncid The file ncid.
 * @param name Variable name.
 * @param xtype NetCDF data type (e.g. NC_FLOAT, NC_INT).
 * @param ndims Number of dimensions.
 * @param dimidsp Array of dimension IDs (length ndims).
 * @param endianness Byte order: NC_ENDIAN_NATIVE, NC_ENDIAN_LITTLE,
 *                   or NC_ENDIAN_BIG.
 * @param type_size Size in bytes of the data type.
 * @param type_name Human-readable type name string (e.g. "float").
 * @param fill_value Optional fill value pointer (may be NULL).
 * @param contiguous Non-zero for contiguous storage, 0 for chunked.
 * @param chunksizes Array of chunk sizes (length ndims), or NULL if
 *                   contiguous.
 * @param format_var_info Format-specific variable info pointer (may be NULL).
 * @param varidp Pointer that receives the new variable ID.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid is invalid.
 * @return NC_ENOMEM if out of memory.
 */
EXTERNL int nc_udf_def_var(int ncid, const char *name, nc_type xtype,
                            int ndims, const int *dimidsp,
                            int endianness, size_t type_size,
                            const char *type_name, const void *fill_value,
                            int contiguous, const size_t *chunksizes,
                            void *format_var_info, int *varidp);

/**
 * Store format-specific variable info pointer.
 *
 * Replaces direct assignment to var->format_var_info.
 *
 * @param ncid The file ncid.
 * @param varid The variable ID.
 * @param format_var_info Pointer to format-specific data (caller owns).
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid or varid is invalid.
 */
EXTERNL int nc_udf_set_var_info(int ncid, int varid, void *format_var_info);

/**
 * Retrieve format-specific variable info pointer.
 *
 * Replaces direct access to var->format_var_info.
 *
 * @param ncid The file ncid.
 * @param varid The variable ID.
 * @param format_var_infop Pointer that receives the format-specific data.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid or varid is invalid.
 * @return NC_EINVAL if format_var_infop is NULL.
 */
EXTERNL int nc_udf_get_var_info(int ncid, int varid, void **format_var_infop);

/**
 * Define an attribute on a variable or the root group of a UDF file.
 *
 * Replaces the need to call nc4_att_list_add() + manual NC_ATT_INFO_T
 * field assignment directly.
 *
 * @param ncid The file ncid.
 * @param varid Variable ID, or NC_GLOBAL for a global attribute.
 * @param name Attribute name.
 * @param xtype NetCDF data type of the attribute.
 * @param len Number of values.
 * @param data Pointer to attribute data.
 *
 * @return NC_NOERR on success.
 * @return NC_EBADID if ncid or varid is invalid.
 * @return NC_ENOMEM if out of memory.
 */
EXTERNL int nc_udf_put_att(int ncid, int varid, const char *name,
                            nc_type xtype, size_t len, const void *data);

#if defined(__cplusplus)
}
#endif

#endif /* NETCDF_UDF_H */
