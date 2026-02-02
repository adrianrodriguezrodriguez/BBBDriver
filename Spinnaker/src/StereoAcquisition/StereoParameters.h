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

#ifndef STEREO_PARAMETERS_H
#define STEREO_PARAMETERS_H

#pragma once

#include "Spinnaker.h"
#include <string>
#include <fstream>
#include <iostream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;

namespace SpinStereo
{
    /** @addtogroup stereo_control
     * @{
     */

    struct StreamTransmitFlags
    {
        bool rawSensor1TransmitEnabled;  ///< Flag to enable raw sensor1 image transmission.
        bool rawSensor2TransmitEnabled;  ///< Flag to enable raw sensor2 image transmission.
        bool rectSensor1TransmitEnabled; ///< Flag to enable rectified sensor1 image transmission.
        bool rectSensor2TransmitEnabled; ///< Flag to enable rectified sensor2 image transmission.
        bool disparityTransmitEnabled;   ///< Flag to enable disparity image transmission.

        std::string ToString() const
        {
            std::stringstream strstr("");

            strstr << "rawSensor1TransmitEnabled " << rawSensor1TransmitEnabled << std::endl;
            strstr << "rawSensor2TransmitEnabled " << rawSensor2TransmitEnabled << std::endl;
            strstr << "rectSensor1TransmitEnabled " << rectSensor1TransmitEnabled << std::endl;
            strstr << "rectSensor2TransmitEnabled " << rectSensor2TransmitEnabled << std::endl;
            strstr << "disparityTransmitEnabled " << disparityTransmitEnabled;

            return strstr.str();
        };
    };

    /**
     * @class StereoParameters
     * @brief Class for handling parameters of the S3D camera.
     */
    class StereoParameters
    {
      public:
        /**
         * @brief Constructor for StereoParameters.
         */
        StereoParameters();

        /**
         * @brief Converts the parameters to a string representation.
         * @return A string representation of the parameters.
         */
        std::string ToString() const;

        float scan3dCoordinateOffset; ///< Minimum number of disparities.
        unsigned int totalDisparity;  ///< Number of disparities.
        bool scan3dInvalidDataFlag;
        float scan3dInvalidDataValue;

        StreamTransmitFlags streamTransmitFlags; ///< Flags to enable streams image transmission.

        bool doComputePointCloud; ///< flag to enable computation of the 3D point cloud.

        int uniquenessRatio; ///< Uniqueness ratio value.
        int smallPenalty;    ///< Small penalty.
        int largePenalty;    ///< Large penalty.

        bool postProcessDisparity; ///< Flag to enable disparity post-processing.

        int maxSpeckleSize;   ///< Speckle range value.
        int speckleThreshold; ///< Speckle threshold value.

        float exposureTime; ///< Exposure time value.
        float gainValue;    ///< Gain value.
        float scan3dFocalLength;
        float scan3dBaseline;
        float scan3dPrincipalPointV;
        float scan3dPrincipalPointU;
        PixelFormatEnums pixelFormat; ///< Pixel format enumeration.

        float scan3dCoordinateScale;
    };

    /** @} */ // end of stereo_control
} // namespace SpinStereo

#endif // STEREO_PARAMETERS_H
