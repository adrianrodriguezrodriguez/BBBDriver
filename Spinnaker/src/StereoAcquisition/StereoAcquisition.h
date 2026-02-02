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

//=============================================================================
// $Id$
//=============================================================================

#ifndef STEREO_ACQUISITION_EXAMPLE_H
#define STEREO_ACQUISITION_EXAMPLE_H

//=============================================================================
// System Includes
//=============================================================================
#include <string>

struct StereoAcquisitionParams
{
    unsigned int numImageSets = 3;
    bool doEnableRawSensor1Transmit = false;
    bool doEnableRawSensor2Transmit = false;
    bool doEnableRectSensor1Transmit = false;
    bool doEnableRectSensor2Transmit = false;
    bool doEnableDisparityTransmit = false;
    bool doEnablePointCloudOutput = false;
    bool doEnableSpeckleFilter = false;

    // Overloading the << operator to implement toString
    friend std::ostream& operator<<(std::ostream& os, const StereoAcquisitionParams& stereoAcquisitionParams)
    {
        os << "numImageSets: " << stereoAcquisitionParams.numImageSets << std::endl
           << "doEnableRawSensor1Transmit: " << stereoAcquisitionParams.doEnableRawSensor1Transmit << std::endl
           << "doEnableRawSensor2Transmit: " << stereoAcquisitionParams.doEnableRawSensor2Transmit << std::endl
           << "doEnableRectSensor1Transmit: " << stereoAcquisitionParams.doEnableRectSensor1Transmit << std::endl
           << "doEnableRectSensor2Transmit: " << stereoAcquisitionParams.doEnableRectSensor2Transmit << std::endl
           << "doEnableDisparityTransmit: " << stereoAcquisitionParams.doEnableDisparityTransmit << std::endl
           << "doEnablePointCloudOutput: " << stereoAcquisitionParams.doEnablePointCloudOutput << std::endl
           << "doEnableSpeckleFilter: " << stereoAcquisitionParams.doEnableSpeckleFilter << std::endl;
        return os;
    }
};

bool ProcessArgs(int argc, char* argv[], StereoAcquisitionParams& params);
void DisplayHelp(const std::string& pszProgramName, const StereoAcquisitionParams& params);

#endif // STEREO_ACQUISITION_EXAMPLE_H
