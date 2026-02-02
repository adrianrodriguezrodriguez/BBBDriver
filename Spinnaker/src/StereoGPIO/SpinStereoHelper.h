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

    bool IsDeviceBumblebeeX(CameraPtr pCam);

    gcstring GetSerialNumber(CameraPtr pCam);

    bool ConfigureGVCPHeartbeat(CameraPtr pCam, bool enableHeartbeat);

    bool ResetGVCPHeartbeat(CameraPtr pCam);

    bool DisableGVCPHeartbeat(CameraPtr pCam);

    bool SetAcquisitionMode(INodeMap& nodeMap);

    bool SetStreamBufferHandlingMode(INodeMap& nodeMapTLDevice);

    bool ConfigureStereoProcessing(INodeMap& nodeMap, StereoParameters& stereoParameters);

    bool ConfigureAcquisition(CameraPtr pCam, StreamTransmitFlags& streamTransmitFlags);

    /**
     * @brief Configures the camera streams.
     * @param doSetTransmittedStreams Flag to set stream enable variable.
     * @return True if configuration is successful, false otherwise.
     * @ingroup camera_control
     */
    bool ConfigureCameraStreams(CameraPtr pCam, StreamTransmitFlags& streamTransmitFlags);

    /**
     * @brief Gets a float value from a specified node.
     * @param nodeName Node name.
     * @param nodeVal Reference to store the retrieved value.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetFloatValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, float& nodeVal);

    /**
     * @brief Gets the max float value for a specified node.
     * @param nodeName Node name.
     * @param nodeVal Reference to store the retrieved max value.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetMaxFloatValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, float& nodeVal);

    /**
     * @brief Enables automatic exposure.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool EnableAutoExposure(CameraPtr pCam);

    /**
     * @brief Retrieves the current exposure time.
     * @param exposureTime Reference to store the exposure time.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetExposureTime(CameraPtr pCam, double& exposureTime);

    /**
     * @brief Gets a boolean value from a specified node.
     * @param nodeName Node name.
     * @param nodeVal Reference to store the retrieved value.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetBooleanValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, bool& nodeVal);

    /**
     * @brief Gets an integer value from a specified node.
     * @param nodeName Node name.
     * @param nodeVal Reference to store the retrieved value.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetIntValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, int64_t& nodeVal);

    /**
     * @brief Gets the max integer value for a specified node.
     * @param nodeName Node name.
     * @param nodeVal Reference to store the retrieved max value.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetMaxIntValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, int64_t& nodeVal);

    /**
     * @brief Gets an enumerated (int) value from a specified node.
     * @param nodeName Node name.
     * @param enumVal Reference to store the retrieved value.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetEnumValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, int64_t& enumVal);

    /**
     * @brief Gets an enumerated string value from a specified node.
     * @param nodeName Node name.
     * @param nodeValAsInt Reference to store the integer value of the node.
     * @param nodeValAsStr Reference to store the string value of the node.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetEnumAsStringValueFromNode(
        INodeMap& nodeMap,
        const gcstring& nodeName,
        int64_t& nodeValAsInt,
        gcstring& nodeValAsStr);

    /**
     * @brief Gets a string value from a specified node.
     * @param nodeName Node name.
     * @param nodeVal Reference to store the retrieved value.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetStringValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, string& nodeVal);

    /**
     * @brief Sets a boolean value to a specified node.
     * @param nodeName Node name.
     * @param isChecked Value to set.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetBooleanValueToNode(INodeMap& nodeMap, const gcstring& nodeName, bool isChecked);

    /**
     * @brief Sets an integer value to a specified node.
     * @param nodeName Node name.
     * @param nodeVal Value to set.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetIntValueToNode(INodeMap& nodeMap, const gcstring& nodeName, int64_t nodeVal);

    /**
     * @brief Sets a double value to a specified node.
     * @param nodeName Node name.
     * @param nodeVal Value to set.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetFloatValueToNode(INodeMap& nodeMap, const gcstring& nodeName, float nodeVal);

    /**
     * @brief Sets an enumerated value to a specified node.
     * @param nodeMap Reference to the node map.
     * @param nodeName Node name.
     * @param nodeVal Value to set.
     * @param nodeMapType Type of the node map.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetEnumValueToNode(
        INodeMap& nodeMap,
        const gcstring& nodeName,
        const int64_t nodeVal,
        NodeMapType nodeMapType = NodeMapType_Camera);

    /**
     * @brief Sets an enumerated string value to a specified node.
     * @param nodeName Node name.
     * @param nodeEnumValAsStr String value to set.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetEnumAsStringValueToNode(INodeMap& nodeMap, const gcstring& nodeName, gcstring nodeEnumValAsStr);

    /**
     * @brief Sets a string value to a specified node.
     * @param nodeName Node name.
     * @param nodeVal Value to set.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetStringValueToNode(INodeMap& nodeMap, const gcstring& nodeName, string& nodeVal);

    /**
     * @brief Retrieves the number of disparities parameter.
     * @param nodeMap the node map to read the number of disparities parameter value from.
     * @param totalDisparity Variable to store the number of disparities parameter.
     * @return True if successful, false otherwise.
     */
    bool GetTotalDisparity(INodeMap& nodeMap, int64_t& totalDisparity);

    /**
     * @brief Retrieves the small penalty parameter.
     * @param nodeMap Reference to the node map.
     * @param smallPenalty Variable to store the small penalty parameter.
     * @return True if successful, false otherwise.
     */
    bool GetSmallPenalty(Spinnaker::GenApi::INodeMap& nodeMap, int64_t& smallPenalty);

    /**
     * @brief Retrieves the large penalty parameter.
     * @param nodeMap Reference to the node map.
     * @param largePenalty Variable to store the large penalty parameter.
     * @return True if successful, false otherwise.
     */
    bool GetLargePenalty(Spinnaker::GenApi::INodeMap& nodeMap, int64_t& largePenalty);

    /**
     * @brief Retrieves the SGBM Uniqueness Ratio parameter.
     * @param nodeMap Reference to the node map.
     * @param uniquenessRatio Variable to store the SGBM Uniqueness Ratio parameter.
     * @return True if successful, false otherwise.
     */
    bool GetUniquenessRatio(Spinnaker::GenApi::INodeMap& nodeMap, int64_t& uniquenessRatio);

    /**
     * @brief Configures the GVCP heartbeat setting.
     * @param doEnable Enable or disable GVCP heartbeat.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool ConfigureGVCPHeartbeat(CameraPtr pCam, bool doEnable);

    /**
     * @brief Disables the GVCP heartbeat.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool DisableGVCPHeartbeat(CameraPtr pCam);

    /**
     * @brief Prints device information using a specific node map.
     * @param nodeMap Reference to the node map.
     * @return Status code of the operation.
     * @ingroup camera_control
     */
    bool PrintDeviceInfo(INodeMap& nodeMap);

    /**
     * @brief Resets the GVCP heartbeat to its default state.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool ResetGVCPHeartbeat();

    /**
     * @brief Sets the exposure time.
     * @param exposureTime Exposure time to set.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetExposureTime(CameraPtr pCam, double exposureTime);

    /**
     * @brief Enables automatic gain control.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool EnableAutoGain(CameraPtr pCam);

    /**
     * @brief Retrieves the current gain value.
     * @param gainValue Reference to store the gain value.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool GetGainValue(CameraPtr pCam, double& gainValue);

    /**
     * @brief Sets the gain value.
     * @param gainValue Gain value to set.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetGainValue(CameraPtr pCam, double gainValue);

    /**
     * @brief Sets the Automatic White Balance (AWB) using specified red and blue values.
     * @param redValue The red balance value to set.
     * @param blueValue The blue balance value to set.
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool SetAutoWhiteBalance(CameraPtr pCam, double redValue, double blueValue);

    /**
     * @brief Enables Automatic White Balance (AWB).
     * @return True if successful, false otherwise.
     * @ingroup camera_control
     */
    bool EnableAutoWhiteBalance(CameraPtr pCam);

    // /**
    //  * @brief Checks that the ImageList is successful and complete
    //  * @return True if successful, false otherwise.
    //  * @ingroup camera_control
    //  */
    bool ValidateImageList(const StreamTransmitFlags& streamTransmitFlags, ImageList& imageList);

    /**
     * @brief Prints the camera calibration parameters.
     * @return True if successful, false otherwise.
     * @ingroup stereo_control
     */
    bool PrintCameraCalibrationParams(INodeMap& nodeMap);

    /** @} */ // end of camera_control

    /** @addtogroup stereo_control
     * @{
     */

    /**
     * @brief Retrieves the camera SGBM related parameters.
     * @return True if successful, false otherwise.
     */
    bool PrintSGBM_params(CameraPtr pCam);

    /**
     * @brief Retrieves the disparity scale factor (the factor to multiply to get from
     * integer value to sub-pixel (floating point) disparity).
     * @param nodeMap the node map to read the scaleFactor parameter value from.
     * @param scaleFactor Variable to store the scale factor for the disparity.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dCoordinateScale(INodeMap& nodeMap, float& scaleFactor);

    /**
     * @brief Retrieves the coordinateOffset parameter.
     * @param nodeMap the node map to read the coordinateOffset parameter value from.
     * @param coordinateOffset Variable to store the min disparity parameter.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dCoordinateOffset(INodeMap& nodeMap, float& coordinateOffset);

    /**
     * @brief Retrieves the scan3dInvalidDataFlag parameter.
     * @param nodeMap the node map to read the invalidDataFlag parameter value from.
     * @param scan3dInvalidDataFlag Variable to store the invalidDataFlag parameter.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dInvalidDataFlag(INodeMap& nodeMap, bool& scan3dInvalidDataFlag);

    /**
     * @brief Retrieves the scan3dInvalidDataValue parameter.
     * @param scan3dInvalidDataValue Variable to store the scan3dInvalidDataValue parameter.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dInvalidDataValue(INodeMap& nodeMap, float& scan3dInvalidDataValue);

    /**
     * @brief Retrieves the focal length of the reference camera.
     * @param focalLength Variable to store the focal length.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dFocalLength(INodeMap& nodeMap, float& focalLength);

    /**
     * @brief Retrieves the image center coordinates of the reference camera in pixels.
     * @param principalPointV Variable to store the row center (cy) of the camera image.
     * @param principalPointU Variable to store the column center (cx) of the camera image.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dPrincipalPoint(INodeMap& nodeMap, float& principalPointV, float& principalPointU);

    /**
     * @brief Retrieves the baseline distance between the stereo cameras in meters.
     * @param baseline Variable to store the baseline distance.
     * @return True if successful, false otherwise.
     */
    bool GetScan3dBaseLine(INodeMap& nodeMap, float& baseline);

    /**
     * @brief Sets up the Semi-Global Block Matching algorithm parameters.
     * @param camParameters Pointer to the camera parameters.
     * @return True if successful, false otherwise.
     */
    bool SetSGBM_params(CameraPtr pCam, StereoParameters& camParameters);

} // namespace SpinStereo

/** @} */ // end of stereo_control

#endif // STEREO_HELPER_H
