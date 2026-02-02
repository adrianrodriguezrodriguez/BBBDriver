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
 *  @example StereoAcquisition.cpp
 *
 *  @brief StereoAcquisition.cpp shows how to acquire image sets from a stereo
 *  camera. The image sets are then saved to file and/or used to compute 3D
 *  point cloud and saved as a PLY (Polygon File Format) file.
 *
 *  This example touches on the preparation and cleanup of a camera just before
 *  and just after the acquisition of images. Image retrieval and conversion,
 *  grabbing image data, and saving images are all covered as well.
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
#include "StereoAcquisition.h"
#include "PointCloud.h"
#include "ImageUtilityStereo.h"

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

/**
 * ProcessArgs
 *
 * ProcessArgs takes command line arguments and modifies the given StereoAcquisitionParams object accordingly.
 *
 * @param[in] argc The number of arguments passed to the program.
 * @param[in] argv The command line arguments passed to the program.
 * @param[in,out] params A StereoAcquisitionParams object to be modified by the function.
 *
 * @return true if the arguments are valid, false otherwise.
 */
bool ProcessArgs(int argc, char* argv[], StereoAcquisitionParams& params)
{
    string executionPath = string(argv[0]);
    size_t found = executionPath.find_last_of("/\\");
    string programName = executionPath.substr(found + 1);

    int iOpt;
    const char* currentParamPosition;
    bool bBadArgs = false;

    // If no arguments run with default parameters.
    if (argc == 1)
    {
        params.doEnableRectSensor1Transmit = true;
        params.doEnableDisparityTransmit = true;
        params.doEnablePointCloudOutput = true;
        params.doEnableSpeckleFilter = true;
        return true;
    }

    const char* paramMatchPattern = "n:ABCDEFGh?";

    while ((iOpt = GetOption(argc, argv, paramMatchPattern, &currentParamPosition)) != 0)
    {
        switch (iOpt)
        {
        //
        // Options
        //
        case 'n':
#ifdef _MSC_VER
            if (sscanf_s(currentParamPosition, "%d", &params.numImageSets) != 1)
#else
            if (sscanf(currentParamPosition, "%d", &params.numImageSets) != 1)
#endif
            {
                bBadArgs = true;
            }
            else
            {
                if (params.numImageSets <= 0)
                {
                    cout << "The numImageSets argument must be a number greater than 0." << endl;
                    bBadArgs = true;
                }
            }
            break;

        case 'A':
            params.doEnableRawSensor1Transmit = true;
            break;

        case 'B':
            params.doEnableRawSensor2Transmit = true;
            break;

        case 'C':
            params.doEnableRectSensor1Transmit = true;
            break;

        case 'D':
            params.doEnableRectSensor2Transmit = true;
            break;

        case 'E':
            params.doEnableDisparityTransmit = true;
            break;

        case 'F':
            params.doEnablePointCloudOutput = true;
            break;

        case 'G':
            params.doEnableSpeckleFilter = true;
            break;

        case '?':
        case 'h':
        default:
            cerr << "Invalid option provided: " << currentParamPosition << endl;
            DisplayHelp(programName, params);
            return false;
        }
    }

    if (bBadArgs)
    {
        cout << "Invalid arguments" << endl;
        DisplayHelp(programName, params);
        return false;
    }
    if (params.doEnablePointCloudOutput)
    {
        if (!params.doEnableDisparityTransmit)
        {
            cout << "Need to have disparity Image (-E) for point cloud generation" << endl << endl;
            DisplayHelp(programName, params);
            return false;
        }
        if (!params.doEnableRectSensor1Transmit)
        {
            cout << "Need to have Rectified Sensor1 Image (-C) for point cloud generation" << endl << endl;
            DisplayHelp(programName, params);
            return false;
        }
    }
    if (!params.doEnableRawSensor1Transmit && !params.doEnableRawSensor2Transmit &&
        !params.doEnableRectSensor1Transmit && !params.doEnableRectSensor2Transmit && !params.doEnableDisparityTransmit)
    {
        cout << "Need to enable at least one image (-A/-B/-C/-D/-E)" << endl << endl;
        DisplayHelp(programName, params);
        return false;
    }

    return true;
}

/**
 * DisplayHelp
 *
 * Display the help for the stereo acquisition example.
 *
 * @param[in] pszProgramName The name of the program.
 * @param[in] params The parameters for the stereo acquisition example.
 */
void DisplayHelp(const string& pszProgramName, const StereoAcquisitionParams& params)
{
    cout << "Usage: ";

    cout << pszProgramName << " [OPTIONS]" << endl << endl;

    cout << "OPTIONS" << endl
         << endl
         << "  -n NUM_FRAMES                        Number frames" << endl
         << "                                       Default is " << params.numImageSets << endl
         << "  -A DO_ENABLE_RAW_SENSOR1_TRANSMIT       doEnableRawSensor1Transmit" << endl
         << "                                       Default is " << params.doEnableRawSensor1Transmit << endl
         << "  -B DO_ENABLE_RAW_SENSOR2_TRANSMIT      doEnableRawSensor2Transmit" << endl
         << "                                       Default is " << params.doEnableRawSensor2Transmit << endl
         << "  -C DO_ENABLE_RECT_SENSOR1_TRANSMIT      doEnableRectSensor1Transmit" << endl
         << "                                       Default is " << params.doEnableRectSensor1Transmit << endl
         << "  -D DO_ENABLE_RECT_SENSOR2_TRANSMIT     doEnableRectSensor2Transmit" << endl
         << "                                       Default is " << params.doEnableRectSensor2Transmit << endl
         << "  -E DO_ENABLE_DISPARITY_TRANSMIT      doEnableDisparityTransmit" << endl
         << "                                       Default is " << params.doEnableDisparityTransmit << endl
         << "  -F DO_ENABLE_POINTCLOUD_OUTPUT       doEnablePointCloudOutput" << endl
         << "                                       Default is " << params.doEnablePointCloudOutput << endl
         << "  -G DO_ENABLE_SPECKLE_FILTER          doEnableSpeckleFilter" << endl
         << "                                       Default is " << params.doEnableSpeckleFilter << endl
         << "EXAMPLE" << endl
         << endl
         << "    " << pszProgramName << " -n " << params.numImageSets << " -A "
         << " -B "
         << " -C "
         << " -D "
         << " -E "
         << " -F " << endl
         << endl;
}

/**
 * Compute3DPointCloudAndSave
 *
 * Compute the 3D point cloud using the image pair and save it to a ply file.
 *
 * @param[in] stereoParameters The stereo parameters.
 * @param[in] imageMap The map of images.
 * @param[in] counter The counter.
 * @param[in] prefix The prefix of the saved file name.
 *
 * @return true if the computation is successful, false otherwise.
 */
bool Compute3DPointCloudAndSave(
    const StereoParameters& stereoParameters,
    ImageList& imageList,
    int counter,
    string prefix = "")
{
    PointCloudParameters pointCloudParameters = PointCloudParameters();
    pointCloudParameters.decimationFactor = 1;
    pointCloudParameters.ROIImageLeft = 0;
    pointCloudParameters.ROIImageTop = 0;
    pointCloudParameters.ROIImageRight = static_cast<unsigned int>(
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1)->GetWidth());
    pointCloudParameters.ROIImageBottom = static_cast<unsigned int>(
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1)->GetHeight());

    StereoCameraParameters stereoCameraParameters = StereoCameraParameters();

    stereoCameraParameters.coordinateOffset = stereoParameters.scan3dCoordinateOffset;
    stereoCameraParameters.baseline = stereoParameters.scan3dBaseline;
    stereoCameraParameters.focalLength = stereoParameters.scan3dFocalLength;
    stereoCameraParameters.principalPointU = stereoParameters.scan3dPrincipalPointU;
    stereoCameraParameters.principalPointV = stereoParameters.scan3dPrincipalPointV;
    stereoCameraParameters.disparityScaleFactor = stereoParameters.scan3dCoordinateScale;
    stereoCameraParameters.invalidDataFlag = stereoParameters.scan3dInvalidDataFlag;
    stereoCameraParameters.invalidDataValue = stereoParameters.scan3dInvalidDataValue;

    PointCloud pointCloud = Spinnaker::ImageUtilityStereo::ComputePointCloud(
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1),
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1),
        pointCloudParameters,
        stereoCameraParameters);

    stringstream strstr("");
    strstr << prefix;
    strstr << "PointCloud_" << counter << ".ply";

    cout << "Save point cloud to file: " << strstr.str() << endl;
    pointCloud.SavePointCloudAsPly(strstr.str().c_str());

    return true;
}

bool SaveImagesToFile(
    const StreamTransmitFlags& streamTransmitFlags,
    ImageList& imageList,
    int counter,
    const string prefix = "")
{
    cout << "Save images to files." << endl;

    stringstream strstr;

    if (streamTransmitFlags.rawSensor1TransmitEnabled)
    {
        strstr.str("");
        strstr << prefix;
        strstr << "RawSensor1_" << counter << ".png";
        string rawSensor1Filename = strstr.str();
        cout << "Save raw Sensor1 image to file: " << rawSensor1Filename << endl;
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR1)->Save(rawSensor1Filename.c_str());
    }

    if (streamTransmitFlags.rawSensor2TransmitEnabled)
    {
        strstr.str("");
        strstr << prefix;
        strstr << "RawSensor2_" << counter << ".png";
        string rawSensor2Filename = strstr.str();
        cout << "Save raw Sensor2 image to file: " << rawSensor2Filename << endl;
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR2)->Save(rawSensor2Filename.c_str());
    }

    if (streamTransmitFlags.rectSensor1TransmitEnabled)
    {
        strstr.str("");
        strstr << prefix;
        strstr << "RectSensor1_" << counter << ".png";
        string rectSensor1Filename = strstr.str();
        cout << "Save rectified sensor1 image to file: " << rectSensor1Filename << endl;
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)->Save(rectSensor1Filename.c_str());
    }

    if (streamTransmitFlags.rectSensor2TransmitEnabled)
    {
        strstr.str("");
        strstr << prefix;
        strstr << "RectSensor2_" << counter << ".png";
        string rectSensor2Filename = strstr.str();
        cout << "Save rectified sensor2 image to file: " << rectSensor2Filename << endl;
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2)->Save(rectSensor2Filename.c_str());
    }

    if (streamTransmitFlags.disparityTransmitEnabled)
    {
        strstr.str("");
        strstr << prefix;
        strstr << "Disparity_" << counter << ".pgm";
        string disparityFilename = strstr.str();
        cout << "Save disparity image to file: " << disparityFilename << endl;
        imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1)->Save(disparityFilename.c_str());
    }

    return true;
}

bool SetDeviceLinkThroughput(CameraPtr pCam)
{
    // NOTE:
    //
    // Camera firmware will report the current bandwidth required by the camera in the 'DeviceLinkCurrentThroughput'
    // node. When 'DeviceLinkThroughputLimit' node is set by the user, firmware will automatically increase packet
    // delay to fulfill the requested throughput. Therefore, once camera is configured for the desired
    // framerate/image size/etc, set 'DeviceLinkCurrentThroughput' node value into 'DeviceLinkThroughputLimit' node,
    // and packet size will be automatically set by the camera.

    INodeMap& nodeMap = pCam->GetNodeMap();

    // Set Stream Channel Packet Size to the max possible on its interface
    CIntegerPtr ptrPacketSize = nodeMap.GetNode("GevSCPSPacketSize");

    if (!IsReadable(ptrPacketSize) || !IsWritable(ptrPacketSize))
    {
        cout << "Unable to read or write packet size. Aborting..." << endl;
        return false;
    }

    unsigned int maxGevSCPSPacketSize = static_cast<unsigned int>(ptrPacketSize->GetMax());
    const unsigned int maxPacketSize =
        pCam->DiscoverMaxPacketSize() > maxGevSCPSPacketSize ? maxGevSCPSPacketSize : pCam->DiscoverMaxPacketSize();

    ptrPacketSize->SetValue(maxPacketSize);

    cout << "PacketSize set to: " << ptrPacketSize->GetValue() << endl;

    CIntegerPtr ptrCurrentThroughput = nodeMap.GetNode("DeviceLinkCurrentThroughput");
    CIntegerPtr ptrThroughputLimit = nodeMap.GetNode("DeviceLinkThroughputLimit");

    if (!IsReadable(ptrCurrentThroughput))
    {
        cout << "Unable to read node DeviceLinkCurrentThroughput. Aborting..." << endl << endl;
        return false;
    }

    if (!IsReadable(ptrThroughputLimit) || !IsWritable(ptrThroughputLimit))
    {
        cout << "Unable to read or write to node DeviceLinkThroughputLimit. Aborting..." << endl;
        return false;
    }

    cout << "Current camera throughput: " << ptrCurrentThroughput->GetValue() << endl;

    // If the 'DeviceLinkCurrentThroughput' value is lower than the minimum, set the lowest possible value allowed
    // by the 'DeviceLinkCurrentThroughput' node
    if (ptrThroughputLimit->GetMin() > ptrCurrentThroughput->GetValue())
    {
        cout << "DeviceLinkCurrentThroughput node minimum of: " << ptrThroughputLimit->GetMin()
             << " is higher than current throughput we desire to set (" << ptrCurrentThroughput->GetValue() << ")"
             << endl;
        ptrThroughputLimit->SetValue(ptrThroughputLimit->GetMin());
    }
    else
    {
        // Set 'DeviceLinkCurrentThroughput' value into 'DeviceLinkThroughputLimit' node so that the
        // camera will adjust inter-packet delay automatically
        ptrThroughputLimit->SetValue(ptrCurrentThroughput->GetValue());
    }

    cout << "DeviceLinkThroughputLimit set to: " << ptrThroughputLimit->GetValue() << endl << endl;

    return true;
}

bool AcquireImages(CameraPtr pCam, StereoParameters& stereoParameters, unsigned int numImageSets)
{
    bool result = true;
    cout << endl << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

    try
    {
        // Begin acquiring images
        pCam->BeginAcquisition();

        cout << endl << "Acquiring " << numImageSets << " image sets." << endl;

        gcstring serialNumber = pCam->TLDevice.DeviceSerialNumber.GetValue();
        uint64_t timeoutInMilliSecs = 2000;

        for (unsigned int counter = 0; counter < numImageSets; counter++)
        {
            try
            {
                cout << endl << "Acquiring stereo image set: " << counter << endl;

                //
                // Retrieve next received set of stereo images
                //
                // *** NOTES ***
                // GetNextImageSync() captures an image from each stream and returns a synchronized
                // image set in an ImageList object based on the frame ID. The ImageList object is
                // simply a generic container for one or more ImagePtr objects.
                //
                // For a set of stereo images, the ImageList object could contain up to five different
                // image payload type images (Raw Sensor1, Raw Sensor2, Rectified Sensor1, Rectified Sensor2 and
                // Disparity Sensor1). The five images that are returned in the ImageList are guaranteed
                // to be synchronized in timestamp and Frame ID.
                //
                // This function cannot be invoked if GetNextImage is being invoked already.
                // Results will be undeterministic and could lead to missing or lost of images.
                //
                // *** LATER ***
                // Once the image list is saved and/or no longer needed, the image list must be
                // released in order to keep the image buffer from filling up.

                ImageList imageList;

                imageList = pCam->GetNextImageSync(timeoutInMilliSecs);

                if (!SpinStereo::ValidateImageList(stereoParameters.streamTransmitFlags, imageList))
                {
                    cout << "Failed to get next image set." << endl;
                    continue;
                }

                if (stereoParameters.postProcessDisparity)
                {
                    if (stereoParameters.streamTransmitFlags.disparityTransmitEnabled)
                    {
                        // Applying SpeckleFilter directly on disparity image
                        cout << "Applying SpeckleFilter on disparity image..." << endl;

                        ImagePtr pDisparity =
                            imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1);
                        ImageUtilityStereo::FilterSpecklesFromImage(
                            pDisparity,
                            stereoParameters.maxSpeckleSize,
                            stereoParameters.speckleThreshold,
                            stereoParameters.scan3dCoordinateScale,
                            stereoParameters.scan3dInvalidDataValue);
                    }
                    else
                    {
                        cout << "Skipping disparity post processing as disparity components are disabled" << endl;
                    }
                }

                stringstream ss("");
                ss << "StereoAcquisition_" << serialNumber << "_";

                if (!SaveImagesToFile(stereoParameters.streamTransmitFlags, imageList, counter, ss.str()))
                {
                    cerr << "Failed to save images." << endl;
                    result = false;
                    break;
                }

                if (stereoParameters.doComputePointCloud)
                {
                    if (stereoParameters.streamTransmitFlags.disparityTransmitEnabled &&
                        stereoParameters.streamTransmitFlags.rectSensor1TransmitEnabled)
                    {
                        // only do if both streams are enabled
                        if (!Compute3DPointCloudAndSave(stereoParameters, imageList, counter, ss.str()))
                        {
                            cerr << "Failed to compute the 3D point cloud." << endl;
                            result = false;
                            break;
                        }
                    }
                    else
                    {
                        cout << "Skipping compute 3D point cloud as rectified sensor1 or disparity sensor1 components "
                                "are disabled"
                             << endl;
                    }
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
bool RunSingleCamera(CameraPtr pCam, StereoParameters& stereoParameters, int numImageSets)
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

        // Camera Stereo Parameters can be configured while camera is acquiring images, but
        // enabling/disabling stream components is only possible before camera begins image acquisition
        cout << endl << "Configuring camera..." << endl;
        result = result && SpinStereo::ConfigureAcquisition(pCam, stereoParameters.streamTransmitFlags);

        cout << endl << "Configuring device link throughput..." << endl;
        result = result && SetDeviceLinkThroughput(pCam);

        cout << endl << "Configuring stereo processing..." << endl;
        result = result && SpinStereo::ConfigureStereoProcessing(nodeMap, stereoParameters);

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
        result = result | AcquireImages(pCam, stereoParameters, numImageSets);

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
    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
    FILE* tempFile = fopen("test.txt", "w+");
    if (tempFile == nullptr)
    {
        cout << "Failed to create file in current folder.  Please check "
                "permissions."
             << endl;
        cout << "Press Enter to exit..." << endl;
        getchar();
        return -1;
    }
    fclose(tempFile);
    remove("test.txt");
    // determine cmd line arguments
    StereoAcquisitionParams stereoAcquisitionParams;
    if (!ProcessArgs(argc, argv, stereoAcquisitionParams))
    {
        return -1;
    }

    StereoParameters stereoParameters;
    StreamTransmitFlags& streamTransmitFlags = stereoParameters.streamTransmitFlags;

    streamTransmitFlags.rawSensor1TransmitEnabled = stereoAcquisitionParams.doEnableRawSensor1Transmit;
    streamTransmitFlags.rawSensor2TransmitEnabled = stereoAcquisitionParams.doEnableRawSensor2Transmit;
    streamTransmitFlags.rectSensor1TransmitEnabled = stereoAcquisitionParams.doEnableRectSensor1Transmit;
    streamTransmitFlags.rectSensor2TransmitEnabled = stereoAcquisitionParams.doEnableRectSensor2Transmit;
    streamTransmitFlags.disparityTransmitEnabled = stereoAcquisitionParams.doEnableDisparityTransmit;
    stereoParameters.doComputePointCloud = stereoAcquisitionParams.doEnablePointCloudOutput;
    stereoParameters.postProcessDisparity = stereoAcquisitionParams.doEnableSpeckleFilter;

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
        result = result | RunSingleCamera(pCam, stereoParameters, stereoAcquisitionParams.numImageSets);

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
