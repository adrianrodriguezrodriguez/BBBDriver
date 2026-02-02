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
    bool IsDeviceBumblebeeX(CameraPtr pCam)
    {
        if (IsReadable(pCam->TLDevice.DeviceModelName))
        {
            std::string deviceModelName = pCam->TLDevice.DeviceModelName.ToString().c_str();
            std::string deviceModelPrefix("Bumblebee X");

            std::size_t found = deviceModelName.find(deviceModelPrefix);
            if (found != std::string::npos)
            {
                // The device is a Bumblebee camera
                return true;
            }
        }
        else
        {
            cerr << "DeviceModelName is not readable" << endl;
        }
        return false;
    }

    gcstring GetSerialNumber(CameraPtr pCam)
    {
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        gcstring deviceSerialNumber("");
        CStringPtr ptrStringSerial = nodeMapTLDevice.GetNode("DeviceSerialNumber");
        if (IsReadable(ptrStringSerial))
        {
            deviceSerialNumber = ptrStringSerial->GetValue();
        }

        return deviceSerialNumber;
    }

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

        CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
        if (!IsReadable(ptrDeviceType))
        {
            return false;
        }

        if (ptrDeviceType->GetIntValue() != DeviceType_GigEVision)
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

        CBooleanPtr ptrDeviceHeartbeat = nodeMap.GetNode("GevGVCPHeartbeatDisable");
        if (!IsWritable(ptrDeviceHeartbeat))
        {
            cout << "Unable to configure heartbeat. Continuing with execution as this may be non-fatal..." << endl
                 << endl;
        }
        else
        {
            ptrDeviceHeartbeat->SetValue(!enableHeartbeat);

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

    bool SetAcquisitionMode(INodeMap& nodeMap)
    {
        //
        // Set acquisition mode to continuous
        //
        // *** NOTES ***
        // Because the example acquires and saves a set number of images,
        // setting acquisition mode to continuous lets the example finish. If
        // set to single frame or multiframe (at a lower number of images),
        // the example would just hang. This would happen because the example
        // has been written to acquire more images than the camera would have
        // been programmed to retrieve.
        //
        // Setting the value of an enumeration node is slightly more complicated
        // than other node types. Two nodes must be retrieved: first, the
        // enumeration node is retrieved from the nodemap; and second, the entry
        // node is retrieved from the enumeration node. The integer value of the
        // entry node is then set as the new value of the enumeration node.
        //
        // Notice that both the enumeration and the entry nodes are checked for
        // availability and readability/writability. Enumeration nodes are
        // generally readable and writable whereas their entry nodes are only
        // ever readable.
        //
        // Retrieve enumeration node from nodemap
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsReadable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous (enum retrieval). Aborting..." << endl << endl;
            return false;
        }

        // Retrieve entry node from enumeration node
        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to get or set acquisition mode to continuous (entry retrieval). Aborting..." << endl
                 << endl;
            return false;
        }

        // Retrieve integer value from entry node
        const int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

        // Set integer value from entry node as new value of enumeration node
        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

        return true;
    }

    bool SetStreamBufferHandlingMode(INodeMap& nodeMapTLDevice)
    {
        // Set the StreamBufferHandlingMode
        CEnumerationPtr ptrHandlingMode = nodeMapTLDevice.GetNode("StreamBufferHandlingMode");
        if (!IsReadable(ptrHandlingMode) || !IsWritable(ptrHandlingMode))
        {
            cout << "Unable to read or write to StreamBufferHandlingMode node. Aborting..." << endl << endl;
            return false;
        }

        cout << "Set Handling mode to OldestFirst" << endl;
        CEnumEntryPtr ptrHandlingModeEntry = ptrHandlingMode->GetEntryByName("OldestFirst");
        if (!IsReadable(ptrHandlingModeEntry))
        {
            cout << "Unable to read ptrHandlingModeEntry node. Aborting..." << endl << endl;
            return false;
        }

        int64_t ptrHandlingModeEntryVal = ptrHandlingModeEntry->GetValue();
        ptrHandlingMode->SetIntValue(ptrHandlingModeEntryVal);

        return true;
    }

    bool ConfigureStereoProcessing(INodeMap& nodeMap, StereoParameters& stereoParameters)
    {
        bool result = true;

        result = result && GetScan3dCoordinateScale(nodeMap, stereoParameters.scan3dCoordinateScale);

        result = result && GetScan3dCoordinateOffset(nodeMap, stereoParameters.scan3dCoordinateOffset);

        result = result && GetScan3dFocalLength(nodeMap, stereoParameters.scan3dFocalLength);

        result = result && GetScan3dBaseLine(nodeMap, stereoParameters.scan3dBaseline);

        result = result &&
                 GetScan3dPrincipalPoint(
                     nodeMap, stereoParameters.scan3dPrincipalPointV, stereoParameters.scan3dPrincipalPointU);

        result = result && GetScan3dInvalidDataFlag(nodeMap, stereoParameters.scan3dInvalidDataFlag);

        result = result && GetScan3dInvalidDataValue(nodeMap, stereoParameters.scan3dInvalidDataValue);

        return result;
    }

    bool ConfigureAcquisition(CameraPtr pCam, StreamTransmitFlags& streamTransmitFlags)
    {
        bool result = true;

        // there are 3 nodeMaps (map to the 3 node maps in SpinView):
        // - nodeMapCamera - the nodemap for the camera
        // - nodeMapTLDevice - the nodemap for the TLDevice
        // - nodeMapTLStream - the nodemap for the TLStream

        // Retrieve Device nodemap
        INodeMap& nodeMapCamera = pCam->GetNodeMap();

        // Retrieve Stream nodemap
        INodeMap& nodeMapTLStream = pCam->GetTLStreamNodeMap();

        result = result && SetAcquisitionMode(nodeMapCamera);

        result = result && SetStreamBufferHandlingMode(nodeMapTLStream);

        // Configure the camera streams Enable var, either on the camera from streamTransmitFlags,
        // or vice versa, depending on doSetTransmittedStreams
        // Initialize imageMap
        result = result && ConfigureCameraStreams(pCam, streamTransmitFlags);

        return result;
    }

    bool GetScan3dCoordinateScale(INodeMap& nodeMap, float& scaleFactor)
    {
        if (!GetFloatValueFromNode(nodeMap, "Scan3dCoordinateScale", scaleFactor))
        {
            std::cerr << "Failed to get the scan3dCoordinateScale parameter from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool GetFloatValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, float& nodeVal)
    {
        CFloatPtr floatPtr = nodeMap.GetNode(nodeName);
        if (!IsReadable(floatPtr))
        {
            std::ostringstream msg;
            msg << nodeName << " is not readable" << endl;
            cerr << msg.str();
            return false;
        }
        nodeVal = static_cast<float>(floatPtr->GetValue());

        return true;
    }

    bool ConfigureCameraStreams(CameraPtr pCam, StreamTransmitFlags& streamTransmitFlags)
    {
        INodeMap& nodeMap = pCam->GetNodeMap();

        try
        {
            CEnumerationPtr ptrSourceSelector = nodeMap.GetNode("SourceSelector");
            CEnumerationPtr ptrComponentSelector = nodeMap.GetNode("ComponentSelector");
            CBooleanPtr ptrComponentEnable = nodeMap.GetNode("ComponentEnable");

            if (!IsReadable(ptrSourceSelector) || !IsWritable(ptrSourceSelector))
            {
                cout << "Unable to set Source Selector (enum retrieval). Aborting..." << endl << endl;
                return false;
            }

            if (!IsReadable(ptrComponentSelector) || !IsWritable(ptrComponentSelector))
            {
                cout << "Unable to set Component Selector (enum retrieval). Aborting..." << endl << endl;
                return false;
            }

            if (!IsReadable(ptrComponentEnable) || !IsWritable(ptrComponentEnable))
            {
                cout << "Unable to set Component Enable. Aborting..." << endl << endl;
                return false;
            }

            CEnumEntryPtr ptrSourceSensor1 = ptrSourceSelector->GetEntryByName("Sensor1");
            CEnumEntryPtr ptrSourceSensor2 = ptrSourceSelector->GetEntryByName("Sensor2");
            CEnumEntryPtr ptrComponentRaw = ptrComponentSelector->GetEntryByName("Raw");
            CEnumEntryPtr ptrComponentRectified = ptrComponentSelector->GetEntryByName("Rectified");
            CEnumEntryPtr ptrComponentDisparity = ptrComponentSelector->GetEntryByName("Disparity");

            if (!IsReadable(ptrSourceSensor1))
            {
                cout << "Unable to set Source Selector to Sensor1 (entry retrieval). Aborting..." << endl << endl;
                return false;
            }

            if (!IsReadable(ptrSourceSensor2))
            {
                cout << "Unable to set Source Selector to Sensor2 (entry retrieval). Aborting..." << endl << endl;
                return false;
            }

            if (!IsReadable(ptrComponentRaw))
            {
                cout << "Unable to set Component Selector to Raw (entry retrieval). Aborting..." << endl << endl;
                return false;
            }

            if (!IsReadable(ptrComponentRectified))
            {
                cout << "Unable to set Component Selector to Rectified (entry retrieval). Aborting..." << endl << endl;
                return false;
            }

            // Disparity is only readable from Sensor1
            ptrSourceSelector->SetIntValue(ptrSourceSensor1->GetValue());
            if (!IsReadable(ptrComponentDisparity))
            {
                cout << "Unable to set Component Selector to Disparity (entry retrieval). Aborting..." << endl << endl;
                return false;
            }

            // Raw Sensor1
            ptrSourceSelector->SetIntValue(ptrSourceSensor1->GetValue());
            ptrComponentSelector->SetIntValue(ptrComponentRaw->GetValue());
            ptrComponentEnable->SetValue(streamTransmitFlags.rawSensor1TransmitEnabled);
            cout << "Raw Sensor 1 set to " << (streamTransmitFlags.rawSensor1TransmitEnabled ? "on" : "off") << endl;

            // Raw Sensor2
            ptrSourceSelector->SetIntValue(ptrSourceSensor2->GetValue());
            ptrComponentSelector->SetIntValue(ptrComponentRaw->GetValue());
            ptrComponentEnable->SetValue(streamTransmitFlags.rawSensor2TransmitEnabled);
            cout << "Raw Sensor 2 set to " << (streamTransmitFlags.rawSensor2TransmitEnabled ? "on" : "off") << endl;

            // Rectified Sensor1
            ptrSourceSelector->SetIntValue(ptrSourceSensor1->GetValue());
            ptrComponentSelector->SetIntValue(ptrComponentRectified->GetValue());
            ptrComponentEnable->SetValue(streamTransmitFlags.rectSensor1TransmitEnabled);
            cout << "Rectified Sensor 1 set to " << (streamTransmitFlags.rectSensor1TransmitEnabled ? "on" : "off")
                 << endl;

            // Rectified Sensor2
            ptrSourceSelector->SetIntValue(ptrSourceSensor2->GetValue());
            ptrComponentSelector->SetIntValue(ptrComponentRectified->GetValue());
            ptrComponentEnable->SetValue(streamTransmitFlags.rectSensor2TransmitEnabled);
            cout << "Rectified Sensor 2 set to " << (streamTransmitFlags.rectSensor2TransmitEnabled ? "on" : "off")
                 << endl;

            // Disparity Sensor1
            ptrSourceSelector->SetIntValue(ptrSourceSensor1->GetValue());
            ptrComponentSelector->SetIntValue(ptrComponentDisparity->GetValue());
            ptrComponentEnable->SetValue(streamTransmitFlags.disparityTransmitEnabled);
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
        INodeMap& nodeMap = pCam->GetNodeMap();

        float scan3dCoordinateOffset;
        if (!GetFloatValueFromNode(nodeMap, "Scan3dCoordinateOffset", scan3dCoordinateOffset))
        {
            std::cerr << "Failed to get the Scan3dCoordinateOffset parameter from the camera." << std::endl;
            return false;
        }

        // the value of Scan3dCoordinateOffsetMaxVal (768) in the camera is derived from (1023 - 255 = 768)
        // 1023 - (2^10) 10 bit for storing the integer part of the disparity
        //        (altough the actual maximum disparity value from the camera is 256 (2^8, i.e. occupies 8 bits out of
        //        the 10)
        // 255 - TotalDisparity (a.k.a. numDisparities) - this value is fixed due to FPGA constraints

        float scanCoordinateOffsetMaxVal;
        if (!GetMaxFloatValueFromNode(nodeMap, "Scan3dCoordinateOffset", scanCoordinateOffsetMaxVal))
        {
            std::cerr << "Failed to get the Scan3dCoordinateOffset max value from the camera." << std::endl;
            return false;
        }

        if (camParameters.scan3dCoordinateOffset > scanCoordinateOffsetMaxVal)
        {
            cerr << "Scan3dCoordinateOffset max value (" << camParameters.scan3dCoordinateOffset
                 << ") is bigger than the maximum possible value: " << scanCoordinateOffsetMaxVal
                 << ". Clamping the value to the maximum possible value." << endl;

            camParameters.scan3dCoordinateOffset = scanCoordinateOffsetMaxVal;
        }

        if (!SetFloatValueToNode(nodeMap, "Scan3dCoordinateOffset", camParameters.scan3dCoordinateOffset))
        {
            std::cerr << "Failed to set the Scan3dCoordinateOffset parameter to the camera." << std::endl;
            return false;
        }

        int64_t uniquenessRatio;
        if (!GetIntValueFromNode(nodeMap, "UniquenessRatio", uniquenessRatio))
        {
            std::cerr << "Failed to get the Uniqueness Ratio parameter from the camera." << std::endl;
            return false;
        }

        if (camParameters.uniquenessRatio != uniquenessRatio)
        {
            if (!SetIntValueToNode(nodeMap, "UniquenessRatio", camParameters.uniquenessRatio))
            {
                std::cerr << "Failed to set the Uniqueness Ratio parameter to the camera." << std::endl;
                return false;
            }
        }

        int64_t smallPenalty;
        if (!GetSmallPenalty(nodeMap, smallPenalty))
        {
            cerr << "Failed to get camera penalty parameter parameter." << endl;
            return false;
        }

        if (camParameters.smallPenalty != smallPenalty)
        {
            if (!SetIntValueToNode(nodeMap, "SmallPenalty", camParameters.smallPenalty))
            {
                std::cerr << "Failed to set the small penalty parameter to the camera." << std::endl;
                return false;
            }
        }

        int64_t largePenalty;
        if (!GetLargePenalty(nodeMap, largePenalty))
        {
            cerr << "Failed to get camera large penalty parameter." << endl;
            return false;
        }

        if (camParameters.largePenalty != largePenalty)
        {
            if (!SetIntValueToNode(nodeMap, "LargePenalty", camParameters.largePenalty))
            {
                std::cerr << "Failed to set the large penalty parameter to the camera." << std::endl;
                return false;
            }
        }

        return true;
    }

    bool EnableAutoExposure(CameraPtr pCam)
    {
        pCam->ExposureAuto.SetValue(ExposureAuto_Continuous);
        return true;
    }

    bool GetExposureTime(CameraPtr pCam, double& exposureTime)
    {
        exposureTime = pCam->ExposureTime.GetValue();
        return true;
    }

    bool GetBooleanValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, bool& nodeVal)
    {
        CBooleanPtr booleanPtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(booleanPtr))
        {
            std::ostringstream msg;
            msg << nodeName << " is not readable" << endl;
            cerr << msg.str();
            return false;
        }

        nodeVal = booleanPtr->GetValue();

        return true;
    }

    bool GetIntValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, int64_t& nodeVal)
    {
        CIntegerPtr intPtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(intPtr))
        {
            cout << nodeName << " is not readable" << endl;
            return false;
        }
        nodeVal = (intPtr->GetValue());

        return true;
    }

    bool GetMaxIntValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, int64_t& nodeVal)
    {
        CIntegerPtr intPtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(intPtr))
        {
            cout << nodeName << " is not readable" << endl;
            return false;
        }
        nodeVal = (intPtr->GetMax());

        return true;
    }

    bool GetMaxFloatValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, float& nodeVal)
    {
        CFloatPtr floatPtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(floatPtr))
        {
            cout << nodeName << " is not readable" << endl;
            return false;
        }
        nodeVal = static_cast<float>(floatPtr->GetMax());

        return true;
    }

    bool GetEnumValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, int64_t& enumVal)
    {
        CEnumerationPtr cEnumerationPtr = nodeMap.GetNode(nodeName);
        CNodePtr pNode = (CNodePtr)cEnumerationPtr;

        if (IsReadable(pNode))
        {
            enumVal = ((CEnumerationPtr)pNode)->GetIntValue();
        }
        else
        {
            cout << "node: " << nodeName << ", is not readable." << endl;
            return false;
        }

        return true;
    }

    bool GetEnumAsStringValueFromNode(
        INodeMap& nodeMap,
        const gcstring& nodeName,
        int64_t& nodeValAsInt,
        gcstring& nodeValAsStr)
    {
        CEnumerationPtr nodePtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(nodePtr))
        {
            std::ostringstream msg;
            msg << "Unable to read " << nodeName << endl;
            cerr << msg.str();
            return false;
        }
        nodeValAsInt = nodePtr->GetIntValue();
        nodeValAsStr = nodePtr->GetEntry(nodeValAsInt)->ToString();

        return true;
    }

    bool GetStringValueFromNode(INodeMap& nodeMap, const gcstring& nodeName, string& nodeVal)
    {
        CStringPtr nodePtr = nodeMap.GetNode(nodeName);
        if (!IsReadable(nodePtr))
        {
            std::ostringstream msg;
            msg << "Node: " << nodeName << " is not readable" << endl;
            cerr << msg.str();
            return false;
        }
        nodeVal = nodePtr->GetValue();

        return true;
    }

    bool SetExposureTime(CameraPtr pCam, double exposureTime)
    {
        pCam->ExposureAuto.SetValue(ExposureAuto_Off);
        if (exposureTime >= 0)
        {
            pCam->ExposureTime.SetValue(exposureTime);
        }

        return true;
    }

    bool EnableAutoGain(CameraPtr pCam)
    {
        pCam->GainAuto.SetValue(GainAuto_Continuous);
        return true;
    }

    bool GetGainValue(CameraPtr pCam, double& gainValue)
    {
        gainValue = pCam->Gain.GetValue();
        return true;
    }

    bool SetGainValue(CameraPtr pCam, double gainValue = 0)
    {
        pCam->GainAuto.SetValue(GainAuto_Off);
        if (gainValue >= 0)
        {
            pCam->Gain.SetValue(gainValue);
        }

        return true;
    }

    bool SetBooleanValueToNode(INodeMap& nodeMap, const gcstring& nodeName, bool isChecked)
    {
        CBooleanPtr booleanPtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(booleanPtr) || !IsWritable(booleanPtr))
        {
            std::ostringstream msg;
            msg << nodeName << " is not readable or writable  " << endl;
            cerr << msg.str();
            return false;
        }
        booleanPtr->SetValue(isChecked);

        return true;
    }

    bool SetIntValueToNode(INodeMap& nodeMap, const gcstring& nodeName, int64_t nodeVal)
    {
        CIntegerPtr intValue = nodeMap.GetNode(nodeName);

        if (!IsReadable(intValue) || !IsWritable(intValue))
        {
            std::ostringstream msg;
            msg << "Unable to read or write " << nodeName << " . Aborting..." << endl;
            cerr << msg.str();
            return false;
        }
        intValue->SetValue(nodeVal);

        return true;
    }

    bool SetFloatValueToNode(INodeMap& nodeMap, const gcstring& nodeName, float nodeVal)
    {
        CFloatPtr nodePtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(nodePtr) || !IsWritable(nodePtr))
        {
            std::ostringstream msg;
            msg << nodeName << "is not readable or writable" << endl;
            cerr << msg.str();
            return false;
        }
        nodePtr->SetValue(nodeVal);

        return true;
    }

    bool SetEnumValueToNode(INodeMap& nodeMap, const gcstring& nodeName, const int64_t enumVal, NodeMapType nodeMapType)
    {
        CEnumerationPtr cEnumerationPtr = nodeMap.GetNode(nodeName);
        CNodePtr pNode = (CNodePtr)cEnumerationPtr;

        if (IsWritable(((CEnumerationPtr)pNode)))
        {
            // set the value
            ((CEnumerationPtr)pNode)->SetIntValue(enumVal);
        }
        else
        {
            cout << "node: " << nodeName << ", is not writable." << endl;
            return false;
        }

        return true;
    }

    bool SetEnumAsStringValueToNode(INodeMap& nodeMap, const gcstring& nodeName, gcstring nodeEnumValAsStr)
    {
        CEnumerationPtr nodePtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(nodePtr) || !IsWritable(nodePtr))
        {
            cerr << "node: " << nodeName.c_str() << " is not readable or not writable" << endl;
            return false;
        }

        CEnumEntryPtr enumNodePtr = nodePtr->GetEntryByName(nodeEnumValAsStr);
        if (!IsReadable(enumNodePtr))
        {
            std::ostringstream msg;
            msg << "Node: " << nodeEnumValAsStr << " is not readable. Aborting..." << endl;
            cerr << msg.str();
            return false;
        }
        const int64_t enumNodeVal = enumNodePtr->GetValue();
        nodePtr->SetIntValue(enumNodeVal);

        return true;
    }

    bool SetStringValueToNode(INodeMap& nodeMap, const gcstring& nodeName, string& nodeVal)
    {
        CStringPtr nodePtr = nodeMap.GetNode(nodeName);

        if (!IsReadable(nodePtr) || !IsWritable(nodePtr))
        {
            std::ostringstream msg;
            msg << "Node: " << nodeName << "is not readable or writable" << endl;
            cerr << msg.str();
            return false;
        }
        nodePtr->SetValue(nodeVal.c_str());

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

    bool GetScan3dCoordinateOffset(INodeMap& nodeMap, float& coordinateOffset)
    {
        if (!GetFloatValueFromNode(nodeMap, "Scan3dCoordinateOffset", coordinateOffset))
        {
            std::cerr << "Failed to get the min disparity parameter from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool GetScan3dInvalidDataFlag(INodeMap& nodeMap, bool& scan3dInvalidDataFlag)
    {
        if (!GetBooleanValueFromNode(nodeMap, "Scan3dInvalidDataFlag", scan3dInvalidDataFlag))
        {
            std::cerr << "Failed to get the Scan3dInvalidDataFlag parameter from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool GetScan3dInvalidDataValue(INodeMap& nodeMap, float& scan3dInvalidDataValue)
    {
        if (!GetFloatValueFromNode(nodeMap, "Scan3dInvalidDataValue", scan3dInvalidDataValue))
        {
            std::cerr << "Failed to get the Scan3dInvalidDataValue from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool GetTotalDisparity(INodeMap& nodeMap, int64_t& totalDisparity)
    {
        if (!GetIntValueFromNode(nodeMap, "TotalDisparity", totalDisparity))
        {
            std::cerr << "Failed to get the number of disparities parameter from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool GetSmallPenalty(Spinnaker::GenApi::INodeMap& nodeMap, int64_t& smallPenalty)
    {
        if (!GetIntValueFromNode(nodeMap, "SmallPenalty", smallPenalty))
        {
            std::cerr << "Failed to get the smallPenalty parameter from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool GetLargePenalty(Spinnaker::GenApi::INodeMap& nodeMap, int64_t& largePenalty)
    {
        if (!GetIntValueFromNode(nodeMap, "LargePenalty", largePenalty))
        {
            std::cerr << "Failed to get the large penalty parameter from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool GetUniquenessRatio(Spinnaker::GenApi::INodeMap& nodeMap, int64_t& uniquenessRatio)
    {
        if (!GetIntValueFromNode(nodeMap, "UniquenessRatio", uniquenessRatio))
        {
            std::cerr << "Failed to get the SGBM Uniqueness Ratio parameter from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool PrintCameraCalibrationParams(INodeMap& nodeMap)
    {
        cout << "Camera calibration parameters: " << endl;
        float baseline, scaleFactor, focalLength, centerRow, centerCol;
        if (!GetScan3dBaseLine(nodeMap, baseline))
        {
            cerr << "Failed to read the baseline from the camera" << endl;
            return false;
        }
        cout << "baseline: " << baseline << endl;

        if (!GetScan3dCoordinateScale(nodeMap, scaleFactor))
        {
            cerr << "Failed to read the scale factor for the disparity from the camera" << endl;
            return false;
        }
        cout << "scaleFactor after round-up: " << scaleFactor << endl;

        if (!GetScan3dFocalLength(nodeMap, focalLength))
        {
            cerr << "Failed to get camera focal length." << endl;
            return false;
        }
        cout << "focal length: " << focalLength << endl;

        if (!GetScan3dPrincipalPoint(nodeMap, centerRow, centerCol))
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

        INodeMap& nodeMap = pCam->GetNodeMap();
        float coordinateOffset;
        if (!GetScan3dCoordinateOffset(nodeMap, coordinateOffset))
        {
            cerr << "Failed to get camera coordinateOffset parameter." << endl;
            return false;
        }
        cout << "coordinateOffset: " << coordinateOffset << endl;

        bool scan3dInvalidDataFlag;
        if (!GetScan3dInvalidDataFlag(nodeMap, scan3dInvalidDataFlag))
        {
            cerr << "Failed to get camera scan3dInvalidDataFlag parameter." << endl;
            return false;
        }
        cout << "scan3dInvalidDataFlag: " << scan3dInvalidDataFlag << endl;

        float scan3dInvalidDataValue;
        if (!GetScan3dInvalidDataValue(nodeMap, scan3dInvalidDataValue))
        {
            cerr << "Failed to get camera scan3dInvalidDataValue parameter." << endl;
            return false;
        }
        cout << "scan3dInvalidDataValue: " << scan3dInvalidDataValue << endl;

        int64_t totalDisparity;
        if (!GetTotalDisparity(nodeMap, totalDisparity))
        {
            cerr << "Failed to get camera Total Disparity parameter." << endl;
            return false;
        }
        cout << "Total Disparity: " << totalDisparity << endl;

        int64_t smallPenalty;
        if (!GetSmallPenalty(nodeMap, smallPenalty))
        {
            cerr << "Failed to get camera small penalty parameter." << endl;
            return false;
        }
        cout << "smallPenalty: " << smallPenalty << endl;

        int64_t largePenalty;
        if (!GetLargePenalty(nodeMap, largePenalty))
        {
            cerr << "Failed to get camera large penalty parameter." << endl;
            return false;
        }
        cout << "largePenalty: " << largePenalty << endl;

        int64_t uniquenessRatio;
        if (!GetUniquenessRatio(nodeMap, uniquenessRatio))
        {
            cerr << "Failed to get camera Uniqueness Ratio parameter." << endl;
            return false;
        }
        cout << "uniquenessRatio: " << uniquenessRatio << endl;

        return true;
    }

    bool GetScan3dFocalLength(INodeMap& nodeMap, float& focalLength)
    {
        if (!GetFloatValueFromNode(nodeMap, "Scan3dFocalLength", focalLength))
        {
            std::cerr << "Failed to get the focalLength from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool GetScan3dPrincipalPoint(INodeMap& nodeMap, float& principalPointV, float& principalPointU)
    {
        if (!GetFloatValueFromNode(nodeMap, "Scan3dPrincipalPointV", principalPointV))
        {
            std::cerr << "Failed to get the Scan3dPrincipalPointV from the camera." << std::endl;
            return false;
        }

        if (!GetFloatValueFromNode(nodeMap, "Scan3dPrincipalPointU", principalPointU))
        {
            std::cerr << "Failed to get the Scan3dPrincipalPointU from the camera." << std::endl;
            return false;
        }

        return true;
    }

    bool GetScan3dBaseLine(INodeMap& nodeMap, float& baseline)
    {
        if (!GetFloatValueFromNode(nodeMap, "Scan3dBaseline", baseline))
        {
            std::cerr << "Failed to get the baseline from the camera." << std::endl;
            return false;
        }
        return true;
    }

    bool PrintDeviceInfo(INodeMap& nodeMap)
    {
        bool result = true;
        cout << endl << "*** DEVICE INFORMATION ***" << endl;

        try
        {
            FeatureList_t features;
            const CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
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

    bool EnableAutoWhiteBalance(CameraPtr pCam)
    {

        pCam->BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Continuous);
        return true;
    }

    bool SetAutoWhiteBalance(CameraPtr pCam, double redVal, double blueVal)
    {
        pCam->BalanceWhiteAuto.SetValue(BalanceWhiteAuto_Off);

        pCam->BalanceRatioSelector.SetValue(BalanceRatioSelector_Red);
        pCam->BalanceRatio.SetValue(redVal);

        pCam->BalanceRatioSelector.SetValue(BalanceRatioSelector_Blue);
        pCam->BalanceRatio.SetValue(blueVal);

        return true;
    }

} // namespace SpinStereo
