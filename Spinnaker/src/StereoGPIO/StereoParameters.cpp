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

#include "StereoParameters.h"
#include <sstream>

namespace SpinStereo
{
    StereoParameters::StereoParameters()
    {
        exposureTime = 20000;
        gainValue = 0.0;
        scan3dFocalLength = 0.0;
        scan3dBaseline = 0.0;
        scan3dPrincipalPointV = 0.0;
        scan3dPrincipalPointU = 0.0;
        scan3dCoordinateOffset = 0.0;
        totalDisparity = 256;
        scan3dInvalidDataFlag = true;
        scan3dInvalidDataValue = 0.0;
        postProcessDisparity = true;
        maxSpeckleSize = 40;
        speckleThreshold = 4;
        uniquenessRatio = 10;
        streamTransmitFlags = {false, false, true, false, true};
        doComputePointCloud = false;
        smallPenalty = 5;
        largePenalty = 60;
        pixelFormat = PixelFormat_RGB8Packed;
        scan3dCoordinateScale = 1.0/64;
    }

    std::string StereoParameters::ToString() const
    {
        std::stringstream strstr("");

        strstr << "exposureTime " << exposureTime << std::endl;
        strstr << "gainValue " << gainValue << std::endl;
        strstr << "scan3dFocalLength " << scan3dFocalLength << std::endl;
        strstr << "scan3dBaseline " << scan3dBaseline << std::endl;
        strstr << "scan3dPrincipalPointV " << scan3dPrincipalPointV << std::endl;
        strstr << "scan3dPrincipalPointU " << scan3dPrincipalPointU << std::endl;
        strstr << "scan3dCoordinateOffset " << scan3dCoordinateOffset << std::endl;
        strstr << "totalDisparity " << totalDisparity << std::endl;
        strstr << "scan3dInvalidDataFlag " << scan3dInvalidDataFlag << std::endl;
        strstr << "scan3dInvalidDataValue " << scan3dInvalidDataValue << std::endl;
        strstr << "postProcessDisparity " << postProcessDisparity << std::endl;
        strstr << "maxSpeckleSize " << maxSpeckleSize << std::endl;
        strstr << "speckleThreshold " << speckleThreshold << std::endl;
        strstr << "uniquenessRatio " << uniquenessRatio << std::endl;
        strstr << streamTransmitFlags.ToString() << std::endl;
        strstr << "doComputePointCloud " << doComputePointCloud << std::endl;
        strstr << "small penalty " << smallPenalty << std::endl;
        strstr << "large penalty " << largePenalty << std::endl;
        strstr << "pixelFormat " << pixelFormat << std::endl;
        strstr << "scan3dCoordinateScale " << scan3dCoordinateScale << std::endl;

        return strstr.str();
    }
} // namespace SpinStereo
