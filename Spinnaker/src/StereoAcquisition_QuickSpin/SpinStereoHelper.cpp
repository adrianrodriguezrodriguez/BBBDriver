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
#include "SpinStereoHelper.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <cmath>

#define ComponentEnabledAsBool

using namespace std;

const unsigned int printEveryNFrames = 50;
unsigned int imageGroupCounter = 0;

namespace SpinStereo
{
    // Disables or enables heartbeat on GEV cameras so debugging does not incur timeout errors
    bool ConfigureGVCPHeartbeat(CameraPtr pCam, bool enableHeartbeat)
    {
        //
        // Write to boolean node controlling the camera's heartbeat
        //
        // *** NOTES ***
        // This applies only to GEV cameras.
        //
        // GEV cameras have a heartbeat built in, but when debugging applications the
        // camera may time out due to its heartbeat. Disabling the heartbeat prevents
        // this timeout from occurring, enabling us to continue with any necessary
        // debugging.
        //
        // *** LATER ***
        // Make sure that the heartbeat is reset upon completion of the debugging.
        // If the application is terminated unexpectedly, the camera may not locked
        // to Spinnaker indefinitely due to the the timeout being disabled.  When that
        // happens, a camera power cycle will reset the heartbeat to its default setting.

        // Retrieve TL device nodemap
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        if (!IsReadable(pCam->TLDevice.DeviceType))
        {
            std::cerr << "Failed to get the DeviceType parameter from the camera." << std::endl;
            return false;
        }

        if (pCam->TLDevice.DeviceType.GetValue() != DeviceType_GigEVision)
        {
            return true;
        }

        if (enableHeartbeat)
        {
            cout << endl << "Resetting heartbeat..." << endl << endl;
        }
        else
        {
            cout << endl << "Disabling heartbeat..." << endl << endl;
        }

        if (!IsReadable(pCam->GevGVCPHeartbeatDisable) || !IsWritable(pCam->GevGVCPHeartbeatDisable))
        {
            std::cerr << "Failed to get or set the GevGVCPHeartbeatDisable parameter from or to the camera." << std::endl;
            return false;
        }

        pCam->GevGVCPHeartbeatDisable.SetValue(!enableHeartbeat);

        if (!enableHeartbeat)
        {
            cout << "WARNING: Heartbeat has been disabled for the rest of this example run." << endl;
            cout << "         Heartbeat will be reset upon the completion of this run.  If the " << endl;
            cout << "         example is aborted unexpectedly before the heartbeat is reset, the" << endl;
            cout << "         camera may need to be power cycled to reset the heartbeat." << endl << endl;
        }
        else
        {
            cout << "Heartbeat has been reset." << endl;
        }

        return true;
    }

    bool ResetGVCPHeartbeat(CameraPtr pCam)
    {
        return ConfigureGVCPHeartbeat(pCam, true);
    }

    bool DisableGVCPHeartbeat(CameraPtr pCam)
    {
        return ConfigureGVCPHeartbeat(pCam, false);
    }

    bool SetStreamBufferHandlingMode(CameraPtr pCam)
    {
        if (!IsWritable(pCam->TLStream.StreamBufferHandlingMode))
        {
            std::cerr << "Failed to set the StreamBufferHandlingMode parameter to the camera." << std::endl;
            return false;
        }

        pCam->TLStream.StreamBufferHandlingMode.SetValue(StreamBufferHandlingMode_OldestFirst);

        return true;
    }

    bool ConfigureStereoProcessing(CameraPtr pCam, StereoParameters& stereoParameters)
    {
        bool result = true;

        result = result && GetScan3dCoordinateScale(pCam, stereoParameters.scan3dCoordinateScale);

        result = result && GetScan3dCoordinateOffset(pCam, stereoParameters.scan3dCoordinateOffset);

        result = result && GetScan3dFocalLength(pCam, stereoParameters.scan3dFocalLength);

        result = result && GetScan3dBaseLine(pCam, stereoParameters.scan3dBaseline);

        result = result &&
                 GetScan3dPrincipalPoint(
                     pCam, stereoParameters.scan3dPrincipalPointV, stereoParameters.scan3dPrincipalPointU);

        result = result && GetScan3dInvalidDataFlag(pCam, stereoParameters.scan3dInvalidDataFlag);

        result = result && GetScan3dInvalidDataValue(pCam, stereoParameters.scan3dInvalidDataValue);

        return result;
    }

    bool ConfigureAcquisition(CameraPtr pCam, StreamTransmitFlags& streamTransmitFlags)
    {
        bool result = true;

        // Retrieve Stream nodemap
        INodeMap& nodeMapTLStream = pCam->GetTLStreamNodeMap();

        // Set acquisition mode to continuous
        if (!IsWritable(pCam->AcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous." << endl;
            return false;
        }

        pCam->AcquisitionMode.SetValue(AcquisitionMode_Continuous);

        result = result && SetStreamBufferHandlingMode(pCam);

        // Configure the camera streams Enable var, either on the camera from streamTransmitFlags,
        // or vice versa, depending on doSetTransmittedStreams
        // Initialize imageMap
        result = result && ConfigureCameraStreams(pCam, streamTransmitFlags);

        return result;
    }

    bool GetScan3dCoordinateScale(CameraPtr pCam, float& scaleFactor)
    {
        if (!IsReadable(pCam->Scan3dCoordinateScale))
        {
            std::cerr << "Failed to get the scan3dCoordinateScale parameter from the camera." << std::endl;
            return false;
        }
        scaleFactor = static_cast<float>(pCam->Scan3dCoordinateScale.GetValue());

        return true;
    }

    bool ConfigureCameraStreams(CameraPtr pCam, StreamTransmitFlags& streamTransmitFlags)
    {
        try
        {
        if (!IsReadable(pCam->SourceSelector) || !IsWritable(pCam->SourceSelector))
        {
            std::cerr << "Failed to get or set the SourceSelector parameter from or to the camera." << std::endl;
            return false;
        }

        if (!IsReadable(pCam->ComponentSelector) || !IsWritable(pCam->ComponentSelector))
        {
            std::cerr << "Failed to get or set the ComponentSelector parameter from or to the camera." << std::endl;
            return false;
        }

        if (!IsReadable(pCam->ComponentEnable) || !IsWritable(pCam->ComponentEnable))
        {
            std::cerr << "Failed to get or set the ComponentEnable parameter from or to the camera." << std::endl;
            return false;
        }


        // Raw Sensor1
        pCam->SourceSelector.SetValue(SourceSelector_Sensor1);
        pCam->ComponentSelector.SetValue(ComponentSelector_Raw);
        pCam->ComponentEnable.SetValue(streamTransmitFlags.rawSensor1TransmitEnabled);
        cout << "Raw Sensor 1 set to " << (streamTransmitFlags.rawSensor1TransmitEnabled ? "on" : "off") << endl;

        // Raw Sensor2
        pCam->SourceSelector.SetValue(SourceSelector_Sensor2);
        pCam->ComponentSelector.SetValue(ComponentSelector_Raw);
        pCam->ComponentEnable.SetValue(streamTransmitFlags.rawSensor2TransmitEnabled);
        cout << "Raw Sensor 2 set to " << (streamTransmitFlags.rawSensor2TransmitEnabled ? "on" : "off") << endl;

        // Rectified Sensor1
        pCam->SourceSelector.SetValue(SourceSelector_Sensor1);
        pCam->ComponentSelector.SetValue(ComponentSelector_Rectified);
        pCam->ComponentEnable.SetValue(streamTransmitFlags.rectSensor1TransmitEnabled);
        cout << "Rectified Sensor 1 set to " << (streamTransmitFlags.rectSensor1TransmitEnabled ? "on" : "off")
                << endl;

        // Rectified Sensor2
        pCam->SourceSelector.SetValue(SourceSelector_Sensor2);
        pCam->ComponentSelector.SetValue(ComponentSelector_Rectified);
        pCam->ComponentEnable.SetValue(streamTransmitFlags.rectSensor2TransmitEnabled);
        cout << "Rectified Sensor 2 set to " << (streamTransmitFlags.rectSensor2TransmitEnabled ? "on" : "off")
                << endl;

        // Disparity Sensor1
        pCam->SourceSelector.SetValue(SourceSelector_Sensor1);
        pCam->ComponentSelector.SetValue(ComponentSelector_Disparity);
        pCam->ComponentEnable.SetValue(streamTransmitFlags.disparityTransmitEnabled);
        cout << "Disparity Sensor 1 set to " << (streamTransmitFlags.disparityTransmitEnabled ? "on" : "off")
                << endl;
        }
        catch (Spinnaker::Exception& se)
        {
            cout << "Unable to enable stereo source and components. Exception : " << se.what() << std::endl;
            return false;
        }

        return true;
    }

    bool SetSGBM_params(CameraPtr pCam, StereoParameters& camParameters)
    {
        float scan3dCoordinateOffset;
        if (!IsReadable(pCam->Scan3dCoordinateOffset))
        {
            std::cerr << "Failed to get the Scan3dCoordinateOffset parameter from the camera." << std::endl;
            return false;
        }
        scan3dCoordinateOffset = static_cast<float>(pCam->Scan3dCoordinateOffset.GetValue()); 

        // the value of Scan3dCoordinateOffsetMaxVal (768) in the camera is derived from (1023 - 255 = 768)
        // 1023 - (2^10) 10 bit for storing the integer part of the disparity
        //        (altough the actual maximum disparity value from the camera is 256 (2^8, i.e. occupies 8 bits out of
        //        the 10)
        // 255 - TotalDisparity (a.k.a. numDisparities) - this value is fixed due to FPGA constraints

        float scanCoordinateOffsetMaxVal;
        if (!IsReadable(pCam->Scan3dCoordinateOffset))
        {
            std::cerr << "Failed to get the Scan3dCoordinateOffset max value from the camera." << std::endl;
            return false;
        }
        scanCoordinateOffsetMaxVal = static_cast<float>(pCam->Scan3dCoordinateOffset.GetMax());

        if (camParameters.scan3dCoordinateOffset > scanCoordinateOffsetMaxVal)
        {
            cerr << "Scan3dCoordinateOffset max value (" << camParameters.scan3dCoordinateOffset
                 << ") is bigger than the maximum possible value: " << scanCoordinateOffsetMaxVal
                 << ". Clamping the value to the maximum possible value." << endl;

            camParameters.scan3dCoordinateOffset = scanCoordinateOffsetMaxVal;
        }

        if (!IsWritable(pCam->Scan3dCoordinateOffset))
        {
            std::cerr << "Failed to set the Scan3dCoordinateOffset parameter to the camera." << std::endl;
            return false;
        }
        pCam->Scan3dCoordinateOffset.SetValue(camParameters.scan3dCoordinateOffset);            

        int64_t uniquenessRatio;
        if (!IsReadable(pCam->UniquenessRatio))
        {
            std::cerr << "Failed to get the UniquenessRatio parameter from the camera." << std::endl;
            return false;
        }
        uniquenessRatio = pCam->UniquenessRatio.GetValue();            

        if (camParameters.uniquenessRatio != uniquenessRatio)
        {
            if (!IsWritable(pCam->UniquenessRatio))
            {
                std::cerr << "Failed to set the UniquenessRatio parameter to the camera." << std::endl;
                return false;
            }
            pCam->UniquenessRatio.SetValue(camParameters.uniquenessRatio);            
        }

        int64_t smallPenalty;
        if (!GetSmallPenalty(pCam, smallPenalty))
        {
            cerr << "Failed to get camera penalty parameter parameter." << endl;
            return false;
        }

        if (camParameters.smallPenalty != smallPenalty)
        {
            if (!IsWritable(pCam->SmallPenalty))
            {
                std::cerr << "Failed to set the SmallPenalty parameter to the camera." << std::endl;
                return false;
            }
            pCam->SmallPenalty.SetValue(camParameters.smallPenalty);            
        }

        int64_t largePenalty;
        if (!GetLargePenalty(pCam, largePenalty))
        {
            cerr << "Failed to get camera large penalty parameter." << endl;
            return false;
        }

        if (camParameters.largePenalty != largePenalty)
        {
            if (!IsWritable(pCam->LargePenalty))
            {
                std::cerr << "Failed to set the LargePenalty parameter to the camera." << std::endl;
                return false;
            }
            pCam->LargePenalty.SetValue(camParameters.largePenalty);            
        }

        return true;
    }

    bool ValidateImageList(const StreamTransmitFlags& streamTransmitFlags, ImageList& imageList)
    {
        try
        {
            bool isImageListIncomplete = false;

            if (streamTransmitFlags.rawSensor1TransmitEnabled)
            {
                ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR1);
                isImageListIncomplete |= (pImage != nullptr ? pImage->IsIncomplete() : true);
            }

            if (streamTransmitFlags.rawSensor2TransmitEnabled)
            {
                ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR2);
                isImageListIncomplete |= (pImage != nullptr ? pImage->IsIncomplete() : true);
            }

            if (streamTransmitFlags.rectSensor1TransmitEnabled)
            {
                ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1);
                isImageListIncomplete |= (pImage != nullptr ? pImage->IsIncomplete() : true);
            }

            if (streamTransmitFlags.rectSensor2TransmitEnabled)
            {
                ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2);
                isImageListIncomplete |= (pImage != nullptr ? pImage->IsIncomplete() : true);
            }

            if (streamTransmitFlags.disparityTransmitEnabled)
            {
                ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1);
                isImageListIncomplete |= (pImage != nullptr ? pImage->IsIncomplete() : true);
            }

            if (isImageListIncomplete)
            {
#define LOG_VERBOSE_INCOMPLETE_IMAGES
#ifdef LOG_VERBOSE_INCOMPLETE_IMAGES
                std::ostringstream msg;
                msg << "Image List is incomplete: " << endl;
                if (streamTransmitFlags.rawSensor1TransmitEnabled)
                {
                    ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR1);
                    string cameraStreamTypeAsStr;
                    msg << "stream: RAW SENSOR1 - " << Image::GetImageStatusDescription(pImage->GetImageStatus())
                        << endl;
                }

                if (streamTransmitFlags.rawSensor2TransmitEnabled)
                {
                    ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR2);
                    string cameraStreamTypeAsStr;
                    msg << "stream: RAW SENSOR2 - " << Image::GetImageStatusDescription(pImage->GetImageStatus())
                        << endl;
                }

                if (streamTransmitFlags.rectSensor1TransmitEnabled)
                {
                    ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1);
                    string cameraStreamTypeAsStr;
                    msg << "stream: RECT SENSOR1 - " << Image::GetImageStatusDescription(pImage->GetImageStatus())
                        << endl;
                }

                if (streamTransmitFlags.rectSensor2TransmitEnabled)
                {
                    ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2);
                    string cameraStreamTypeAsStr;
                    msg << "stream: RECT SENSOR2 - " << Image::GetImageStatusDescription(pImage->GetImageStatus())
                        << endl;
                }

                if (streamTransmitFlags.disparityTransmitEnabled)
                {
                    ImagePtr pImage = imageList.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1);
                    string cameraStreamTypeAsStr;
                    msg << "stream: DISPARITY SENSOR1 - " << Image::GetImageStatusDescription(pImage->GetImageStatus())
                        << endl;
                }

                msg << "..." << endl;
                cout << msg.str();
#else
                cout << "Image List is incomplete." << endl;
#endif

                return false;
            }

#if 0
            if(imageGroupCounter % printEveryNFrames == 0)
            {
                // print every printEveryNFrames frames
                PrintStreamIndexMap(streamIndexMap);

                if(!PrintSGBM_params(pCam))
                {
                    cerr << "Failed to get camera disparity configuration parameters." << endl;
                    return false;
                }
            }
#endif

            return true;
        }
        catch (Spinnaker::Exception& se)
        {
            cout << "Spinnaker error: " << se.what() << endl;
            return false;
        }
        catch (std::exception e)
        {
            // everything else
            std::cout << "Unhandled exception caught" << std::endl;
            std::cout << e.what() << std::endl;
            return false;
        }
    }

    bool GetScan3dCoordinateOffset(CameraPtr pCam, float& coordinateOffset)
    {
        if (!IsReadable(pCam->Scan3dCoordinateOffset))
        {
            std::cerr << "Failed to get the Scan3dCoordinateOffset parameter from the camera." << std::endl;
            return false;
        }
        coordinateOffset = static_cast<float>(pCam->Scan3dCoordinateOffset.GetValue());

        return true;
    }

    bool GetScan3dInvalidDataFlag(CameraPtr pCam, bool& scan3dInvalidDataFlag)
    {
        if (!IsReadable(pCam->Scan3dInvalidDataFlag))
        {
            std::cerr << "Failed to get the Scan3dInvalidDataFlag parameter from the camera." << std::endl;
            return false;
        }
        scan3dInvalidDataFlag = pCam->Scan3dInvalidDataFlag.GetValue();            

        return true;
    }

    bool GetScan3dInvalidDataValue(CameraPtr pCam, float& scan3dInvalidDataValue)
    {
        if (!IsReadable(pCam->Scan3dInvalidDataValue))
        {
            std::cerr << "Failed to get the Scan3dInvalidDataValue parameter from the camera." << std::endl;
            return false;
        }
        scan3dInvalidDataValue = static_cast<float>(pCam->Scan3dInvalidDataValue.GetValue());

        return true;
    }

    bool GetTotalDisparity(CameraPtr pCam, int64_t& totalDisparity)
    {
        if (!IsReadable(pCam->TotalDisparity))
        {
            std::cerr << "Failed to get the TotalDisparity parameter from the camera." << std::endl;
            return false;
        }
        totalDisparity = pCam->TotalDisparity.GetValue();            

        return true;
    }

    bool GetSmallPenalty(CameraPtr pCam, int64_t& smallPenalty)
    {
        if (!IsReadable(pCam->SmallPenalty))
        {
            std::cerr << "Failed to get the SmallPenalty parameter from the camera." << std::endl;
            return false;
        }
        smallPenalty = pCam->SmallPenalty.GetValue();            

        return true;
    }

    bool GetLargePenalty(CameraPtr pCam, int64_t& largePenalty)
    {
        if (!IsReadable(pCam->LargePenalty))
        {
            std::cerr << "Failed to get the LargePenalty parameter from the camera." << std::endl;
            return false;
        }
        largePenalty = pCam->LargePenalty.GetValue();            

        return true;
    }

    bool GetUniquenessRatio(CameraPtr pCam, int64_t& uniquenessRatio)
    {
        if (!IsReadable(pCam->UniquenessRatio))
        {
            std::cerr << "Failed to get the UniquenessRatio parameter from the camera." << std::endl;
            return false;
        }
        uniquenessRatio = pCam->UniquenessRatio.GetValue();            

        return true;
    }

    bool PrintCameraCalibrationParams(CameraPtr pCam)
    {
        cout << "Camera calibration parameters: " << endl;
        float baseline, scaleFactor, focalLength, centerRow, centerCol;
        if (!GetScan3dBaseLine(pCam, baseline))
        {
            cerr << "Failed to read the baseline from the camera" << endl;
            return false;
        }
        cout << "baseline: " << baseline << endl;

        if (!GetScan3dCoordinateScale(pCam, scaleFactor))
        {
            cerr << "Failed to read the scale factor for the disparity from the camera" << endl;
            return false;
        }
        cout << "scaleFactor after round-up: " << scaleFactor << endl;

        if (!GetScan3dFocalLength(pCam, focalLength))
        {
            cerr << "Failed to get camera focal length." << endl;
            return false;
        }
        cout << "focal length: " << focalLength << endl;

        if (!GetScan3dPrincipalPoint(pCam, centerRow, centerCol))
        {
            cerr << "Failed to get camera image centers." << endl;
            return false;
        }
        cout << "image centers: " << centerRow << ", " << centerCol << endl;

        return true;
    }

    bool PrintSGBM_params(CameraPtr pCam)
    {
        cout << "SGBM params: " << endl;

        float coordinateOffset;
        if (!GetScan3dCoordinateOffset(pCam, coordinateOffset))
        {
            cerr << "Failed to get camera coordinateOffset parameter." << endl;
            return false;
        }
        cout << "coordinateOffset: " << coordinateOffset << endl;

        bool scan3dInvalidDataFlag;
        if (!GetScan3dInvalidDataFlag(pCam, scan3dInvalidDataFlag))
        {
            cerr << "Failed to get camera scan3dInvalidDataFlag parameter." << endl;
            return false;
        }
        cout << "scan3dInvalidDataFlag: " << scan3dInvalidDataFlag << endl;

        float scan3dInvalidDataValue;
        if (!GetScan3dInvalidDataValue(pCam, scan3dInvalidDataValue))
        {
            cerr << "Failed to get camera scan3dInvalidDataValue parameter." << endl;
            return false;
        }
        cout << "scan3dInvalidDataValue: " << scan3dInvalidDataValue << endl;

        int64_t totalDisparity;
        if (!GetTotalDisparity(pCam, totalDisparity))
        {
            cerr << "Failed to get camera Total Disparity parameter." << endl;
            return false;
        }
        cout << "Total Disparity: " << totalDisparity << endl;

        int64_t smallPenalty;
        if (!GetSmallPenalty(pCam, smallPenalty))
        {
            cerr << "Failed to get camera small penalty parameter." << endl;
            return false;
        }
        cout << "smallPenalty: " << smallPenalty << endl;

        int64_t largePenalty;
        if (!GetLargePenalty(pCam, largePenalty))
        {
            cerr << "Failed to get camera large penalty parameter." << endl;
            return false;
        }
        cout << "largePenalty: " << largePenalty << endl;

        int64_t uniquenessRatio;
        if (!GetUniquenessRatio(pCam, uniquenessRatio))
        {
            cerr << "Failed to get camera UniquenessRatio parameter." << endl;
            return false;
        }
        cout << "uniquenessRatio: " << uniquenessRatio << endl;

        return true;
    }

    bool GetScan3dFocalLength(CameraPtr pCam, float& focalLength)
    {
        if (!IsReadable(pCam->Scan3dFocalLength))
        {
            std::cerr << "Failed to get the focalLength parameter from the camera." << std::endl;
            return false;
        }
        focalLength = static_cast<float>(pCam->Scan3dFocalLength.GetValue());

        return true;
    }

    bool GetScan3dPrincipalPoint(CameraPtr pCam, float& principalPointV, float& principalPointU)
    {
        if (!IsReadable(pCam->Scan3dPrincipalPointV))
        {
            std::cerr << "Failed to get the Scan3dPrincipalPointV parameter from the camera." << std::endl;
            return false;
        }
        principalPointV = static_cast<float>(pCam->Scan3dPrincipalPointV.GetValue());

        if (!IsReadable(pCam->Scan3dPrincipalPointU))
        {
            std::cerr << "Failed to get the Scan3dPrincipalPointU parameter from the camera." << std::endl;
            return false;
        }
        principalPointU = static_cast<float>(pCam->Scan3dPrincipalPointU.GetValue());

        return true;
    }

    bool GetScan3dBaseLine(CameraPtr pCam, float& baseline)
    {
        if (!IsReadable(pCam->Scan3dBaseline))
        {
            std::cerr << "Failed to get the baseline parameter from the camera." << std::endl;
            return false;
        }
        baseline = static_cast<float>(pCam->Scan3dBaseline.GetValue());

        return true;
    }

    bool PrintDeviceInfo(CameraPtr pCam)
    {
        bool result = true;
        cout << endl << "*** DEVICE INFORMATION ***" << endl;

        try
        {
            FeatureList_t features;

            INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
            const CCategoryPtr category = nodeMapTLDevice.GetNode("DeviceInformation");
            if (IsReadable(category))
            {
                category->GetFeatures(features);

                for (auto it = features.begin(); it != features.end(); ++it)
                {
                    const CNodePtr pfeatureNode = *it;
                    cout << pfeatureNode->GetName() << " : ";
                    CValuePtr pValue = static_cast<CValuePtr>(pfeatureNode);
                    cout
                        << (IsReadable(pValue) ? pValue->ToString()
                                               : (Spinnaker::GenICam::gcstring) "Node not readable");
                    cout << endl;
                }
            }
            else
            {
                cout << "Device control information not available." << endl;
            }
        }
        catch (Spinnaker::Exception& e)
        {
            cout << "Error: " << e.what() << endl;
            result = false;
        }

        return result;
    }

} // namespace SpinStereo
