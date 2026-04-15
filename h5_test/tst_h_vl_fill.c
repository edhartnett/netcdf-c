/* This is part of the netCDF package. Copyright 2026 University
   Corporation for Atmospheric Research/Unidata See COPYRIGHT file for
   conditions of use.

   HDF5-only reproduction of netcdf-c GitHub issue #2212.

   Bug: When a chunked VLEN dataset has a user-defined fill value and
   not all chunks are written, H5Dread fails with "no write intent on
   file" if the file was opened read-only (H5F_ACC_RDONLY).

   Root cause: HDF5 materializes fill values for unwritten chunks
   during the read path. For VLEN types the conversion code
   (H5D__fill_refill_vl -> H5T__conv_vlen -> H5T__vlen_disk_write ->
   H5VL__native_blob_put -> H5HG_insert) tries to insert a blob into
   the global heap, which is a write operation and fails on a
   read-only file.

   Opening the same file H5F_ACC_RDWR makes the read succeed because
   the global-heap write is permitted, but that is not acceptable
   behavior — a read should never require write access.

   Observed in HDF5 1.12.1 and 2.1.0.

   Ed Hartnett, 4/15/26
*/

#include "h5_err_macros.h"
#include <hdf5.h>

#define FILE_NAME "tst_h_vl_fill.h5"
#define DIM_LEN 4
#define WRITE_COUNT 2

int
main()
{
    printf("\n*** Checking HDF5 VLEN with fill value and chunking (issue #2212).\n");

    printf("*** Checking chunked VLEN(double) + custom fill + partial write...");
    {
        hid_t fileid, spaceid, typeid, datasetid, plistid, mem_spaceid;
        hsize_t dim = DIM_LEN;
        hsize_t chunk = 1;
        hvl_t data[WRITE_COUNT], fill, rdata[DIM_LEN];
        double d0[] = {2, 5};
        double d1[] = {88, 96, 42};
        double fv[] = {0, 101};
        hsize_t start, count;
        herr_t status;
        int i;

        data[0].len = 2; data[0].p = d0;
        data[1].len = 3; data[1].p = d1;
        fill.len = 2; fill.p = fv;

        /* Create the file. */
        if ((fileid = H5Fcreate(FILE_NAME, H5F_ACC_TRUNC, H5P_DEFAULT,
                                H5P_DEFAULT)) < 0) ERR;

        /* Create VLEN type of doubles. */
        if ((typeid = H5Tvlen_create(H5T_NATIVE_DOUBLE)) < 0) ERR;

        /* Create dataspace with fixed dimension of 4. */
        if ((spaceid = H5Screate_simple(1, &dim, NULL)) < 0) ERR;

        /* Set up chunking and fill value. */
        if ((plistid = H5Pcreate(H5P_DATASET_CREATE)) < 0) ERR;
        if (H5Pset_chunk(plistid, 1, &chunk) < 0) ERR;
        status = H5Pset_fill_value(plistid, typeid, &fill);
        if (status < 0) ERR;

        /* Create the dataset. */
        if ((datasetid = H5Dcreate2(fileid, "var", typeid, spaceid,
                                    H5P_DEFAULT, plistid, H5P_DEFAULT)) < 0) ERR;

        /* Write only the first 2 of 4 elements. */
        start = 0; count = WRITE_COUNT;
        if ((mem_spaceid = H5Screate_simple(1, &count, NULL)) < 0) ERR;
        if (H5Sselect_hyperslab(spaceid, H5S_SELECT_SET, &start, NULL,
                                &count, NULL) < 0) ERR;
        if (H5Dwrite(datasetid, typeid, mem_spaceid, spaceid, H5P_DEFAULT,
                     data) < 0) ERR;

        if (H5Sclose(mem_spaceid) < 0) ERR;
        if (H5Dclose(datasetid) < 0) ERR;
        if (H5Sclose(spaceid) < 0) ERR;
        if (H5Pclose(plistid) < 0) ERR;
        if (H5Tclose(typeid) < 0) ERR;
        if (H5Fclose(fileid) < 0) ERR;

        /* Reopen and read all 4 elements. Elements 2 and 3 were never
         * written, so HDF5 must supply the fill value for them. */
        if ((fileid = H5Fopen(FILE_NAME, H5F_ACC_RDONLY, H5P_DEFAULT)) < 0) ERR;
        if ((datasetid = H5Dopen2(fileid, "var", H5P_DEFAULT)) < 0) ERR;
        if ((typeid = H5Dget_type(datasetid)) < 0) ERR;
        if ((spaceid = H5Dget_space(datasetid)) < 0) ERR;

        memset(rdata, 0, sizeof(rdata));
        status = H5Dread(datasetid, typeid, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                         rdata);
        if (status < 0) {
            printf("\n\t*** H5Dread failed — HDF5 VLEN fill bug present\n");
            ERR;
        }

        /* Verify written elements. */
        if (rdata[0].len != 2) ERR;
        if (((double *)rdata[0].p)[0] != 2.0) ERR;
        if (((double *)rdata[0].p)[1] != 5.0) ERR;
        if (rdata[1].len != 3) ERR;
        if (((double *)rdata[1].p)[0] != 88.0) ERR;
        if (((double *)rdata[1].p)[1] != 96.0) ERR;
        if (((double *)rdata[1].p)[2] != 42.0) ERR;

        /* Verify fill elements got the custom fill value. */
        if (rdata[2].len != 2) ERR;
        if (((double *)rdata[2].p)[0] != 0.0) ERR;
        if (((double *)rdata[2].p)[1] != 101.0) ERR;
        if (rdata[3].len != 2) ERR;
        if (((double *)rdata[3].p)[0] != 0.0) ERR;
        if (((double *)rdata[3].p)[1] != 101.0) ERR;

        /* Reclaim HDF5-allocated memory. */
#if H5_VERSION_GE(1,12,0)
        if (H5Treclaim(typeid, spaceid, H5P_DEFAULT, rdata) < 0) ERR;
#else
        if (H5Dvlen_reclaim(typeid, spaceid, H5P_DEFAULT, rdata) < 0) ERR;
#endif

        if (H5Dclose(datasetid) < 0) ERR;
        if (H5Sclose(spaceid) < 0) ERR;
        if (H5Tclose(typeid) < 0) ERR;
        if (H5Fclose(fileid) < 0) ERR;
    }
    SUMMARIZE_ERR;

    FINAL_RESULTS;
}
