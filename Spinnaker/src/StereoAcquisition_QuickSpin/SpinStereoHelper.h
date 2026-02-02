//=============================================================================
// Copyright (c) 2025 FLIR Integrated Imaging Solutions, Inc. All Rights
// Reserved
//
// This software is the confidential and proprietary information of FLIR
// Integrated Imaging Solutions, Inc. ("Confidential Information"). You
// shall not disclose such Confidential Information and shall use it only in
// accordance with the terms of the non-disclosure or license agreement you
// entered into with FLIR Integrated Imaging Solutions, Inc. (FLIR).
//
// FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================

#ifndef STEREO_HELPER_H
#define STEREO_HELPER_H

/**
 * @file SpinStereoHelper.h
 * @brief Header file that includes definitions for the SpinStereoHelper namespace functions.
 */

/**
 * @defgroup camera_control Camera Control
 * @brief Functions and classes for controlling the camera.
 */

/**
 * @defgroup stereo_control Stereo Control
 * @brief Functions and classes for controlling stereo-specific parameters.
 */

#pragma once
#include "Spinnaker.h"
#include "StereoParameters.h"

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

namespace SpinStereo
{

    typedef enum NodeMapType
    {
        NodeMapType_Camera = 0,
        NodeMapType_TLStream = 1
    } NodeMapType;

    bool ConfigureGVCPHeartbeat(CameraPtr pCam, bool enableHeartbeat);

    bool ResetGVCPHeartbeat(CameraPtr pCam);

    bool DisableGVCPHeartbeat(CameraPtr pCam);

    bool SetStreamBufferHandlingMode(CameraPtr pCam);

    bool ConfigureStereoProcessing(CameraPtr pCam, StereoParameters& stereoParameters);

    bool ConfigureAcquisition(CameraPtr pCam, StreamTransmitFlags& streamTransmitFlags);

    /**
     * @brief Configures the camera streams.
     * @param pCam Reference to the camera.
     * @param doSetTransmittedStreams Flag to set stream enable variable.
     * @return True if configuration is successful, false otherwise.
     * @ingroup camera_control
     */
    bool ConfigureCameraStreams(CameraPtr pCam, StreamTransmitFlags& streamTransmitFlags);

    /**
     * @brief Retrieves the disparity scale factor (the factor to multiply to get from
     * integer value to sub-pixel (floating point) disparity).
     * @param pCam Reference to the camera.
     * @param scaleFactor Variable to store the scale factor for the disparity.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dCoordinateScale(CameraPtr pCam, float& scaleFactor);

    /**
     * @brief Retrieves the coordinateOffset parameter.
     * @param pCam Reference to the camera.
     * @param coordinateOffset Variable to store the min disparity parameter.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dCoordinateOffset(CameraPtr pCam, float& coordinateOffset);

    /**
     * @brief Retrieves the scan3dInvalidDataFlag parameter.
     * @param pCam Reference to the camera.
     * @param scan3dInvalidDataFlag Variable to store the invalidDataFlag parameter.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dInvalidDataFlag(CameraPtr pCam, bool& scan3dInvalidDataFlag);

    /**
     * @brief Retrieves the scan3dInvalidDataValue parameter.
     * @param pCam Reference to the camera.
     * @param scan3dInvalidDataValue Variable to store the scan3dInvalidDataValue parameter.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dInvalidDataValue(CameraPtr pCam, float& scan3dInvalidDataValue);

    /**
     * @brief Retrieves the number of disparities parameter.
     * @param pCam Reference to the camera.
     * @param totalDisparity Variable to store the number of disparities parameter.
     * @return True if successful, false otherwise.
     */
    bool GetTotalDisparity(CameraPtr pCam, int64_t& totalDisparity);

    /**
     * @brief Retrieves the small penalty parameter.
     * @param pCam Reference to the camera.
     * @param smallPenalty Variable to store the small penalty parameter.
     * @return True if successful, false otherwise.
     */
    bool GetSmallPenalty(CameraPtr pCam, int64_t& smallPenalty);

    /**
     * @brief Retrieves the large penalty parameter.
     * @param pCam Reference to the camera.
     * @param largePenalty Variable to store the large penalty parameter.
     * @return True if successful, false otherwise.
     */
    bool GetLargePenalty(CameraPtr pCam, int64_t& largePenalty);

    /**
     * @brief Retrieves the SGBM Uniqueness Ratio parameter.
     * @param pCam Reference to the camera.
     * @param uniquenessRatio Variable to store the SGBM Uniqueness Ratio parameter.
     * @return True if successful, false otherwise.
     */
    bool GetUniquenessRatio(CameraPtr pCam, int64_t& uniquenessRatio);

    /**
     * @brief Configures the GVCP heartbeat setting.
     * @param pCam Reference to the camera.
     * @param doEnable Enable or disable GVCP heartbeat.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool ConfigureGVCPHeartbeat(CameraPtr pCam, bool doEnable);

    /**
     * @brief Disables the GVCP heartbeat.
     * @param pCam Reference to the camera.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool DisableGVCPHeartbeat(CameraPtr pCam);

    /**
     * @brief Prints device information using a specific node map.
     * @param pCam Reference to the camera.
     * @return Status code of the operation.
     * @ingroup camera_control
     */
    bool PrintDeviceInfo(CameraPtr pCam);

    /**
     * @brief Resets the GVCP heartbeat to its default state.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool ResetGVCPHeartbeat();

    // /**
    //  * @brief Checks that the ImageList is successful and complete
    //  * @return True if successful, false otherwise.
    //  * @ingroup camera_control
    //  */
    bool ValidateImageList(const StreamTransmitFlags& streamTransmitFlags, ImageList& imageList);

    /**
     * @brief Prints the camera calibration parameters.
     * @param pCam Reference to the camera.
     * @return True if successful, false otherwise.
     * @ingroup stereo_control
     */
    bool PrintCameraCalibrationParams(CameraPtr pCam);

    /** @} */ // end of camera_control

    /** @addtogroup stereo_control
     * @{
     */

    /**
     * @brief Retrieves the camera SGBM related parameters.
     * @param pCam Reference to the camera.
     * @return True if successful, false otherwise.
     */
    bool PrintSGBM_params(CameraPtr pCam);

    /**
     * @brief Retrieves the focal length of the reference camera.
     * @param pCam Reference to the camera.
     * @param focalLength Variable to store the focal length.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dFocalLength(CameraPtr pCam, float& focalLength);

    /**
     * @brief Retrieves the image center coordinates of the reference camera in pixels.
     * @param pCam Reference to the camera.
     * @param principalPointV Variable to store the row center (cy) of the camera image.
     * @param principalPointU Variable to store the column center (cx) of the camera image.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dPrincipalPoint(CameraPtr pCam, float& principalPointV, float& principalPointU);

    /**
     * @brief Retrieves the baseline distance between the stereo cameras in meters.
     * @param pCam Reference to the camera.
     * @param baseline Variable to store the baseline distance.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dBaseLine(CameraPtr pCam, float& baseline);

    /**
     * @brief Sets up the Semi-Global Block Matching algorithm parameters.
     * @param pCam Reference to the camera.
     * @param camParameters Pointer to the camera parameters.
     * @return True if successful, false otherwise.
     */
    bool SetSGBM_params(CameraPtr pCam, StereoParameters& camParameters);

} // namespace SpinStereo

/** @} */ // end of stereo_control

#endif // STEREO_HELPER_H
