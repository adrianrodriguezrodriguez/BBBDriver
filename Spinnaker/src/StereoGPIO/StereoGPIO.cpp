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

/**
 *  @example StereoGPIO.cpp
 *
 *  @brief StereoGPIO.cpp shows how to set the GPIO of the stereo camera.
 *
 *  This example demonstrates how to set the GPIO of the camera.
 *  After setting the GPIO controls, images are grabbed pending on a signal
 *  in the GPIO line
 *
 *  Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
 *  More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
 *  Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics
 */

//=============================================================================
// System Includes
//=============================================================================
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <sstream>
#include <filesystem>

//=============================================================================
// Examples Includes
//=============================================================================
#include "StereoGPIO.h"
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include "SpinStereoHelper.h"
#include "StereoParameters.h"
#include "Getopt.h"

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace SpinStereo;
using namespace std;

bool ProcessArgs(int argc, char* argv[], StereoGPIOParams& params)
{
    string executionPath = string(argv[0]);
    size_t found = executionPath.find_last_of("/\\");
    string programName = executionPath.substr(found + 1);

    int iOpt;
    const char* currentParamPosition;

    // If no arguments run with default parameters.
    if (argc == 1)
    {
        return true;
    }

    const char* paramMatchPattern = "h?";

    while ((iOpt = GetOption(argc, argv, paramMatchPattern, &currentParamPosition)) != 0)
    {
        switch (iOpt)
        {
        case '?':
        case 'h':
        {
            DisplayHelp(programName, params);
            return false;
        }
        default:
            cerr << "Invalid option provided: " << currentParamPosition << endl;
            DisplayHelp(programName, params);
            return false;
        }
    }

    return true;
}

void DisplayHelp(const string& pszProgramName, const StereoGPIOParams& params)
{
    cout << "Usage: ";

    cout << pszProgramName << " [OPTIONS]" << endl << endl;

    cout << "OPTIONS" << endl << endl << "EXAMPLE" << endl << endl << "    " << pszProgramName << endl << endl;
}

bool ConfigureGPIO(CameraPtr pCam)
{
    bool result = true;

    INodeMap& nodeMap = pCam->GetNodeMap();

    // Set Trigger mode to Off
    // Trigger mode must be off when setting the trigger source.
    result = result && SpinStereo::SetEnumAsStringValueToNode(nodeMap, "TriggerMode", "Off");
    cout << "Trigger mode disabled." << endl;

    // Set trigger source to line 0
    result = result && SpinStereo::SetEnumAsStringValueToNode(nodeMap, "TriggerSource", "Line0");
    cout << "Trigger source set to hardware, line 0." << endl;

    // Set srigger selector to frame start
    result = result && SpinStereo::SetEnumAsStringValueToNode(nodeMap, "TriggerSelector", "FrameStart");
    cout << "Trigger selector set to frame start." << endl;

    // Set line selector to line 1
    result = result && SpinStereo::SetEnumAsStringValueToNode(nodeMap, "LineSelector", "Line1");
    cout << "Line selector set line 1." << endl;

    // Set line source to exposure active
    result = result && SpinStereo::SetEnumAsStringValueToNode(nodeMap, "LineSource", "ExposureActive");
    cout << "Line source set to exposure active." << endl;

    // Set Trigger mode to On
    result = result && SpinStereo::SetEnumAsStringValueToNode(nodeMap, "TriggerMode", "On");
    cout << "Trigger mode enabled." << endl;

    return result;
}

bool AcquireImages(CameraPtr pCam, const StreamTransmitFlags& streamTransmitFlags, unsigned int numImageSets)
{
    bool result = true;
    cout << endl << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

    try
    {
        // Begin acquiring images
        pCam->BeginAcquisition();

        gcstring serialNumber = pCam->TLDevice.DeviceSerialNumber.GetValue();
        uint64_t timeoutInMilliSecs = 5000;

        cout << "Acquiring " << numImageSets << " image sets pending on GPIO signal,";
        if (timeoutInMilliSecs == EVENT_TIMEOUT_INFINITE)
        {
            cout << " within an infinite time limit." << endl;
        }
        else
        {
            cout << " within a time limit of " << timeoutInMilliSecs / 1000 << " secs." << endl;
        }

        for (unsigned int counter = 0; counter < numImageSets; counter++)
        {
            try
            {
                cout << endl << "Acquiring stereo image set: " << counter << ", pending on GPIO signal." << endl;

                ImageList imageList;
                imageList = pCam->GetNextImageSync(timeoutInMilliSecs);
                if (!SpinStereo::ValidateImageList(streamTransmitFlags, imageList))
                {
                    cout << "Failed to get next image set." << endl;
                    continue;
                }
            }
            catch (Spinnaker::Exception& e)
            {
                cout << "Error: " << e.what() << endl;
                result = false;
            }
        }

        //
        // End acquisition
        //
        // *** NOTES ***
        // Ending acquisition appropriately helps ensure that devices clean up
        // properly and do not need to be power-cycled to maintain integrity.
        //
        pCam->EndAcquisition();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = false;
    }

    return result;
}

// This function acts as the body of the example; please see NodeMapInfo example
// for more in-depth comments on setting up cameras.
bool RunSingleCamera(CameraPtr pCam, StereoParameters& stereoParameters, unsigned int numImageSets)
{
    bool result = true;

    try
    {
        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        result = PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Check to make sure camera supports stereo vision
        cout << endl << "Checking camera stereo support..." << endl;
        if (!ImageUtilityStereo::IsStereoCamera(pCam))
        {
            cout << "Device serial number " << pCam->TLDevice.DeviceSerialNumber.GetValue()
                 << " is not a valid BX camera. Skipping..." << endl;
            return true;
        }

        // Configure heartbeat for GEV camera
#ifdef _DEBUG
        result = result && DisableGVCPHeartbeat(pCam);
#else
        result = result && ResetGVCPHeartbeat(pCam);
#endif

        cout << endl << "Configuring camera..." << endl;
        result = result && SpinStereo::ConfigureAcquisition(pCam, stereoParameters.streamTransmitFlags);

        cout << endl << "Configuring GPIO..." << endl;
        result = result && ConfigureGPIO(pCam);

        cout << endl << "*** STEREO PARAMETERS *** " << endl << stereoParameters.ToString() << endl;

#if _DEBUG
        cout << endl << "*** CAMERA CALIBRATION PARAMETERS ***" << endl;
        if (!PrintCameraCalibrationParams(nodeMap))
        {
            cerr << "Failed to get camera calibration parameters." << endl;
            return false;
        }
#endif

        // Acquire images
        cout << endl << "Acquiring images..." << endl;
        result = result | AcquireImages(pCam, stereoParameters.streamTransmitFlags, numImageSets);

        // Set trigger to Off
        result = result | SpinStereo::SetEnumAsStringValueToNode(nodeMap, "TriggerMode", "Off");
        cout << "Trigger mode disabled." << endl;


#ifdef _DEBUG
        // Reset heartbeat for GEV camera
        result = result && ResetGVCPHeartbeat(pCam);
#endif

        // Deinitialize camera
        pCam->DeInit();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = false;
    }

    return result;
}

int main(int argc, char** argv)
{
    // determine cmd line arguments
    StereoGPIOParams stereoGPIOParams;
    if (!ProcessArgs(argc, argv, stereoGPIOParams))
    {
        return -1;
    }

    StereoParameters stereoParameters;
    StreamTransmitFlags& streamTransmitFlags = stereoParameters.streamTransmitFlags;

    streamTransmitFlags.rectSensor1TransmitEnabled = stereoGPIOParams.doEnableRectSensor1Transmit;
    streamTransmitFlags.disparityTransmitEnabled = stereoGPIOParams.doEnableDisparityTransmit;

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
         << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
         << endl;

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    const unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << endl << endl;

    // Finish if there are no cameras
    if (numCameras == 0)
    {
        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << "Not enough cameras!" << endl;
        cout << "Done! Press Enter to exit..." << endl;
        getchar();

        return -1;
    }

    //
    // Create shared pointer to camera
    //
    // *** NOTES ***
    // The CameraPtr object is a shared pointer, and will generally clean itself
    // up upon exiting its scope. However, if a shared pointer is created in the
    // same scope that a system object is explicitly released (i.e. this scope),
    // the reference to the shared point must be broken manually.
    //
    // *** LATER ***
    // Shared pointers can be terminated manually by assigning them to nullptr.
    // This keeps releasing the system from throwing an exception.
    //
    CameraPtr pCam = nullptr;

    bool result = true;

    // Run example on each camera
    for (unsigned int i = 0; i < numCameras; i++)
    {
        // Select camera
        pCam = camList.GetByIndex(i);

        cout << endl << "Running example for camera " << i << "..." << endl;

        // Run example
        result = result | RunSingleCamera(pCam, stereoParameters, stereoGPIOParams.numImageSets);

        cout << "Camera " << i << " example complete..." << endl << endl;
    }

    //
    // Release reference to the camera
    //
    // *** NOTES ***
    // Had the CameraPtr object been created within the for-loop, it would not
    // be necessary to manually break the reference because the shared pointer
    // would have automatically cleaned itself up upon exiting the loop.
    //
    pCam = nullptr;

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return (result == true) ? 0 : -1;
}
