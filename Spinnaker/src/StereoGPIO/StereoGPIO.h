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

#ifndef STEREO_GPIO_EXAMPLE_H
#define STEREO_GPIO_EXAMPLE_H

//=============================================================================
// System Includes
//=============================================================================
#include <string>

struct StereoGPIOParams
{
    int numImageSets = 3;
    bool doEnableRectSensor1Transmit = true;
    bool doEnableDisparityTransmit = true;

    // Overloading the << operator to implement toString
    friend std::ostream& operator<<(std::ostream& os, const StereoGPIOParams& stereoGPIOParams)
    {
        os << "doEnableRectSensor1Transmit: " << stereoGPIOParams.doEnableRectSensor1Transmit << std::endl
           << "doEnableDisparityTransmit: " << stereoGPIOParams.doEnableDisparityTransmit << std::endl;
        return os;
    }
};

bool ProcessArgs(int argc, char* argv[], StereoGPIOParams& params);
void DisplayHelp(const std::string& pszProgramName, const StereoGPIOParams& params);

#endif // STEREO_GPIO_EXAMPLE_H
