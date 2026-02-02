//=============================================================================
// Copyright (c) 2025 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of FLIR
// Integrated Imaging Solutions, Inc. ("Confidential Information"). You
// shall not disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with FLIR Integrated Imaging Solutions, Inc. (FLIR).
//
// FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================

/**
*  @example DecompressionGPU.cpp
*
*  @brief DecompressionGPU.cpp shows how to acquire images from multiple cameras 
*  simultaneously using threads and decompress images using the SpinnakerGPU class
*  and saves output images for visual inspection.
*  
*  A compatible NVIDIA Graphics Card and CUDA 10 or above is required for this
*  example to build and run properly.
*  
*  Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
*  More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
*  Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics
*/

#include "stdafx.h"

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include "SpinnakerGPU.h"

#include <iostream>
#include <sstream>
#include <fstream>

#include "cuda_runtime_api.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/time.h>
#endif

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// Target Decompressed Image Dimensions
const unsigned int TARGET_WIDTH = 2448;
const unsigned int TARGET_HEIGHT = 2048;

namespace HighPrecision
{
    // High precision timer
    class Timer
    {
    public:
        struct Query
        {
            inline static unsigned long long GetFrequencyTicks()
            {
#if defined(_WIN32) || defined(_WIN64)
                LARGE_INTEGER freq;
                QueryPerformanceFrequency(&freq);
                return static_cast<long long&>(freq.QuadPart);
#endif
            }


#if defined(_WIN32) || defined(_WIN64)
            inline static unsigned long long GetTicks()
            {
                LARGE_INTEGER ticks;
                QueryPerformanceCounter(&ticks);
                return static_cast<long long&>(ticks.QuadPart);
            }
#else
            inline static timeval GetTicks()
            {
                timeval currentTime;
                gettimeofday(&currentTime, nullptr);
                return currentTime;
            }
#endif

        private:
            Query();
        };

        Timer()
        {
            m_ticksStart = Query::GetTicks();
        }

        inline double GetSeconds() const
        {
#if defined(_WIN32) || defined(_WIN64)
            const unsigned long long ticksNow = Query::GetTicks();
            const unsigned long long ticksElapsed = ticksNow - m_ticksStart;
            return static_cast<double>(ticksElapsed) / s_frequencyDoublePrecision();
#else
            const timeval ticksNow = Query::GetTicks();
            double elapsedTime = (ticksNow.tv_sec - m_ticksStart.tv_sec) * 1000000.0;
            elapsedTime += ticksNow.tv_usec - m_ticksStart.tv_usec;
            return elapsedTime / 1000000.0;
#endif
        }

        inline void Reset()
        {
            m_ticksStart = Query::GetTicks();
        }

    private:
        inline static const unsigned long long& s_frequencyRaw()
        {
            static const unsigned long long s_frequency_data = Query::GetFrequencyTicks();
            return s_frequency_data;
        }

        inline static const double& s_frequencyDoublePrecision()
        {
            static const double s_frequency_data = static_cast<double>(Query::GetFrequencyTicks());
            return s_frequency_data;
        }

#if defined(_WIN32) || defined(_WIN64)
        unsigned long long m_ticksStart;
#else
        timeval m_ticksStart;
#endif
    };
} // namespace HighPrecision

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap& nodeMap, std::string camSerial)
{
    int result = 0;

    cout << "[" << camSerial << "] Printing device information ..." << endl << endl;

    FeatureList_t features;
    CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
    if (IsReadable(category))
    {
        category->GetFeatures(features);

        FeatureList_t::const_iterator it;
        for (it = features.begin(); it != features.end(); ++it)
        {
            CNodePtr pfeatureNode = *it;
            CValuePtr pValue = (CValuePtr)pfeatureNode;
            cout << "[" << camSerial << "] " << pfeatureNode->GetName() << " : "
                << (IsReadable(pValue) ? pValue->ToString() : "Node not readable") << endl;
        }
    }
    else
    {
        cout << "[" << camSerial << "] "
            << "Device control information not readable." << endl;
    }

    cout << endl;

    return result;
}

// Disables or enables heartbeat on GEV cameras so debugging does not incur timeout errors
int ConfigureGVCPHeartbeat(CameraPtr pCam, bool enableHeartbeat)
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
        return -1;
    }

    if (ptrDeviceType->GetIntValue() != DeviceType_GigEVision)
    {
        return 0;
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
        cout << "Unable to configure heartbeat. Continuing with execution as this may be non-fatal..."
            << endl
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

    return 0;
}

int ResetGVCPHeartbeat(CameraPtr pCam)
{
    return ConfigureGVCPHeartbeat(pCam, true);
}

int DisableGVCPHeartbeat(CameraPtr pCam)
{
    return ConfigureGVCPHeartbeat(pCam, false);
}

int ConfigureUserBuffers(CameraPtr pCam, unsigned char* pMemBuffersContiguous)
{
    // Retrieve Camera device nodemap
    const INodeMap& nodeMap = pCam->GetNodeMap();

    // Retrieve Stream Parameters device nodemap
    const INodeMap& sNodeMap = pCam->GetTLStreamNodeMap();

    // Set stream buffer Count Mode to manual
    CEnumerationPtr ptrStreamBufferCountMode = sNodeMap.GetNode("StreamBufferCountMode");
    if (!IsReadable(ptrStreamBufferCountMode) ||
        !IsWritable(ptrStreamBufferCountMode))
    {
        cout << "Unable to get or set Buffer Count Mode (node retrieval). Aborting..." << endl << endl;
        return -1;
    }

    CEnumEntryPtr ptrStreamBufferCountModeManual = ptrStreamBufferCountMode->GetEntryByName("Manual");
    if (!IsReadable(ptrStreamBufferCountModeManual))
    {
        cout << "Unable to get Buffer Count Mode entry (Entry retrieval). Aborting..." << endl << endl;
        return -1;
    }

    ptrStreamBufferCountMode->SetIntValue(ptrStreamBufferCountModeManual->GetValue());

    cout << "Stream Buffer Count Mode set to manual..." << endl;

    CIntegerPtr ptrPayloadSize = nodeMap.GetNode("PayloadSize");
    if (!IsReadable(ptrPayloadSize))
    {
        cout << "Unable to determine the payload size from the nodemap. Aborting..." << endl << endl;
        return -1;
    }
    uint64_t bufferSize = ptrPayloadSize->GetValue();

    // Calculate the 1024 aligned image size to be used for USB cameras
    CEnumerationPtr ptrDeviceType = pCam->GetTLDeviceNodeMap().GetNode("DeviceType");
    if (ptrDeviceType != nullptr && ptrDeviceType->GetIntValue() == DeviceType_USB3Vision)
    {
        const uint64_t usbPacketSize = 1024;
        bufferSize = ((bufferSize + usbPacketSize - 1) / usbPacketSize) * usbPacketSize;
    }
    cout << "bufferSize = " << ptrPayloadSize->GetValue() << endl;

    // Contiguous memory buffer
    const unsigned int numBuffers = 5;
    const unsigned int totalSize = static_cast<unsigned int>(sizeof(unsigned char)*numBuffers*bufferSize);

    //
    // Allocate pinned host buffers for improved decompression performance
    //
    // *** NOTES ***
    //
    // Allocates totalSize bytes of host memory that is page-locked and accessible to the device. The graphics
    // driver tracks the virtual memory ranges allocated with this function and automatically accelerates calls
    // to functions such as cudaMemcpy(). Since the memory can be accessed directly by the GPU device, it can 
    // be read or written with much higher bandwidth than pageable memory obtained with functions such as malloc(). 
    // Allocating excessive amounts of pinned memory may degrade system performance, since it reduces the amount of 
    // memory available to the system for paging.

    cudaError cuError = cudaSuccess;
    try
    {
        cuError = cudaHostAlloc(reinterpret_cast<void**>(&pMemBuffersContiguous), totalSize, cudaHostAllocDefault);
        if (cuError != cudaSuccess)
        {
            cout << "Failed to allocate necessary pinned host buffers. Aborting..." << endl;
            return -1;
        }
    }
    catch (bad_alloc&)
    {
        cout << "Unable to allocate the memory required. Aborting..." << endl << endl;
        return -1;
    }

    pCam->SetUserBuffers(pMemBuffersContiguous, numBuffers * bufferSize);

    // Set buffer ownership to user.
    // This must be set before using user buffers when calling BeginAcquisition().
    // If not set, BeginAcquisition() will use the system's buffers.
    if (pCam->GetBufferOwnership() != SPINNAKER_BUFFER_OWNERSHIP_USER)
    {
        pCam->SetBufferOwnership(SPINNAKER_BUFFER_OWNERSHIP_USER);
    }

    return 0;
}

// This function acquires and saves 100 images from a camera.
#if defined(_WIN32)
DWORD WINAPI AcquireImages(LPVOID lpParam)
{
    CameraPtr pCam = *((CameraPtr*)lpParam);
#else
void* AcquireImages(void* arg)
{
    CameraPtr pCam = *((CameraPtr*)arg);
#endif

    try
    {
        // Retrieve TL device nodemap
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        // Retrieve device serial number for filename
        CStringPtr ptrStringSerial = nodeMapTLDevice.GetNode("DeviceSerialNumber");

        std::string serialNumber = "";

        if (IsReadable(ptrStringSerial))
        {
            serialNumber = ptrStringSerial->GetValue();
        }

        cout << endl
            << "[" << serialNumber << "] "
            << "*** IMAGE ACQUISITION THREAD STARTING"
            << " ***" << endl
            << endl;

        // Print device information
        PrintDeviceInfo(nodeMapTLDevice, serialNumber);

        // Initialize camera
        pCam->Init();

        // Retrieve nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

#ifdef _DEBUG
        // Disable heartbeat for GEV camera for Debug mode
        if (DisableGVCPHeartbeat(pCam) != 0)
#else
        // Reset heartbeat for GEV camera for Release mode
        if (ResetGVCPHeartbeat(pCam) != 0)
#endif
        {
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        //
        // Set acquisition mode to continuous
        //
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsReadable(ptrAcquisitionMode) ||
            !IsWritable(ptrAcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous (node retrieval; camera " << serialNumber
                << "). Aborting..." << endl
                << endl;
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to get acquisition mode to continuous (entry 'continuous' retrieval " << serialNumber
                << "). Aborting..." << endl
                << endl;
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

        cout << "[" << serialNumber << "] "
            << "Acquisition mode set to continuous..." << endl;

        //
        // Set pixel format
        //
        CEnumerationPtr ptrPixelFormat = nodeMap.GetNode("PixelFormat");
        if (IsReadable(ptrPixelFormat) && IsWritable(ptrPixelFormat))
        {
            // Retrieve the desired entry node from the enumeration node
            CEnumEntryPtr ptrPixelFormatBayerRG8 = ptrPixelFormat->GetEntryByName("BayerRG8");
            if (IsReadable(ptrPixelFormatBayerRG8))
            {
                // Retrieve the integer value from the entry node
                int64_t pixelFormatBayerRG8 = ptrPixelFormatBayerRG8->GetValue();

                // Set integer as new value for enumeration node
                ptrPixelFormat->SetIntValue(pixelFormatBayerRG8);

                cout << "Pixel format set to " << ptrPixelFormat->GetCurrentEntry()->GetSymbolic() << "..." << endl;
            }
            else
            {
                cout << "Pixel format BayerRG8 not readable..." << endl;
            }
        }
        else
        {
            cout << "Pixel format not readable or writable..." << endl;
        }

        //
        // Set target width
        //
        CIntegerPtr ptrWidth = nodeMap.GetNode("Width");
        if (!IsWritable(ptrWidth))
        {
            cout << "Width not available. Aborting.." << endl;
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        ptrWidth->SetValue(TARGET_WIDTH);
        cout << "Width set to " << ptrWidth->GetValue() << "..." << endl;

        //
        // Set target height
        //
        CIntegerPtr ptrHeight = nodeMap.GetNode("Height");
        if (!IsWritable(ptrHeight))
        {
            cout << "Height not available. Aborting..." << endl << endl;
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        ptrHeight->SetValue(TARGET_HEIGHT);
        cout << "Height set to " << ptrHeight->GetValue() << "..." << endl << endl;

        //
        // Enable compression
        //
        CEnumerationPtr ptrCompressionMode = nodeMap.GetNode("ImageCompressionMode");
        if (!IsWritable(ptrHeight))
        {
            cout << "Image compression mode not available. Aborting..." << endl << endl;
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        CEnumEntryPtr ptrCompressionModeLossless = ptrCompressionMode->GetEntryByName("Lossless");
        if (!IsReadable(ptrCompressionModeLossless))
        {
            cout << "Unable to set compression mode to Lossless (entry retrieval). Aborting..." << endl << endl;
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        const int64_t compressionModeLossless = ptrCompressionModeLossless->GetValue();
        ptrCompressionMode->SetIntValue(compressionModeLossless);
        cout << "Compression mode set to Lossless..." << endl;

        //
        // Set LosslessCompressionBlockSize if available
        //
        // NOTES: Reducing the size of compressed blocks increases the number of blocks broken down for each
        // and improves decompression performance on the GPU
        CIntegerPtr pLLCBlockSize = nodeMap.GetNode("LosslessCompressionBlockSize");
        if (IsAvailable(pLLCBlockSize) && IsWritable(pLLCBlockSize))
        {
            // 6400 is default
            const int64_t value = (pLLCBlockSize->GetMin() == 276 ? 276 : pLLCBlockSize->GetMin());
            pLLCBlockSize->SetValue(value);

            cout << "LosslessCompressionBlockSize set to " << value << endl;
        }
        else
        {
            cout << "LosslessCompressionBlockSize is not available on this camera, skipping..." << endl;
        }

        //
        // Allocate user buffers
        //
        // *** NOTES ***
        //
        // Since the memory can be accessed directly by the GPU device, it can be read or written with much 
        // higher bandwidth than pageable memory obtained with functions such as malloc(). Allocating excessive
        // amounts of pinned memory may degrade system performance, since it reduces the amount of memory 
        // available to the system for paging.
        //
        // When allocating memory for user buffers, keep in mind that implicitly you are specifying how many
        // buffers are used for the acquisition engine.  There are two ways to set user buffers for Spinnaker
        // to utilize.  You can either pass a pointer to a contiguous buffer, or pass a pointer to pointers to
        // non-contiguous buffers into the library.  In either case, you will be responsible for allocating and
        // de-allocating the memory buffers that the pointers point to.
        //
        // The acquisition engine will be utilizing a bufferCount equal to totalSize divided by bufferSize,
        // where totalSize is the total allocated memory in bytes, and bufferSize is the image payload size.
        //
        // This example here demonstrates how to determine how much memory needs to be allocated based on the
        // retrieved payload size from the node map for both cases.

        unsigned char* pMemBuffersContiguous = nullptr;
        if (ConfigureUserBuffers(pCam, pMemBuffersContiguous) !=0)
        {
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        //
        // Allocate pinned host destination buffers for improved decompression performance
        //
        // *** NOTES ***
        //
        // Allocates hostOutputImageSize bytes of host memory that is page-locked and accessible to the device. The graphics
        // driver tracks the virtual memory ranges allocated with this function and automatically accelerates calls
        // to functions such as cudaMemcpy(). Since the memory can be accessed directly by the GPU device, it can 
        // be read or written with much higher bandwidth than pageable memory obtained with functions such as malloc(). 
        // Allocating excessive amounts of pinned memory may degrade system performance, since it reduces the amount of 
        // memory available to the system for paging.
        //
        // Re-using the same allocated decompressed buffer is much faster than letting the library create new buffer for every
        // grabbed image and then storing the decompressed data.

        const uint64_t offsetX = 0;
        const uint64_t offsetY = 0;
        const PixelFormatEnums pixelFormat = PixelFormat_BayerRG8;
        const unsigned int hostOutputImageSize = static_cast<size_t>(sizeof(uint8_t) * TARGET_WIDTH * TARGET_HEIGHT);
        unsigned char* pHostOutputImageBuffer;
        cudaError cuError = cudaHostAlloc(reinterpret_cast<void**>(&pHostOutputImageBuffer), hostOutputImageSize, cudaHostAllocDefault);
        if (cuError != cudaSuccess)
        {
            cout << "Error allocating host output image buffer: " << cudaGetErrorString(cuError) << endl;
#if defined(_WIN32)
            return 0;
#else
            return (void*)0;
#endif
        }

        ImagePtr pDecompressedImage = Image::Create(TARGET_WIDTH, TARGET_HEIGHT, offsetX, offsetY, pixelFormat, pHostOutputImageBuffer);

        HighPrecision::Timer grabTimer;
        double timeSum = 0;

        // Instantiate and instance of the SpinnakerGPU context
        SpinnakerGPU decompressor;

        // Begin acquiring images
        pCam->BeginAcquisition();

        cout << "[" << serialNumber << "] "
            << "Started acquiring images..." << endl;

        //
        // Retrieve, convert, and save images for each camera
        //
        const unsigned int k_numImages = 100;


        cout << endl;

        for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
        {
            try
            {
                // Retrieve next received image and ensure image completion
                ImagePtr pResultImage = pCam->GetNextImage(1000);

                if (pResultImage->IsIncomplete())
                {
                    cout << "[" << serialNumber << "] "
                        << "Image incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl
                        << endl;
                }
                else
                {
                    // Print image information
                    cout << "[" << serialNumber << "] "
                        << "Grabbed image " << imageCnt << ", width = " << pResultImage->GetWidth()
                        << ", height = " << pResultImage->GetHeight() << "." << endl;
                    //
                    // Fetch compressed image information to pass into the decompression function
                    //
                    //
                    // *** NOTES ***
                    // Two images are decoded at the same time to optimize the decoding performance
                    //
                    cout << "[" << serialNumber << "] " << "Decoding images" << endl;

                    grabTimer.Reset();
                    decompressor.Decompress(pResultImage, pDecompressedImage);
                    double timeCost = grabTimer.GetSeconds();
                    timeSum += timeCost;
                    cout << "[" << serialNumber << "] " << "Image: " << imageCnt << " - Time cost:" << timeCost << endl;

                    // Create a unique filename
                    std::ostringstream filename;

                    filename << "DecompressionGPU-";
                    if (!serialNumber.empty())
                    {
                        filename << serialNumber.c_str();
                    }

                    filename << "-" << imageCnt << ".jpg";

                    // Save image
                    pDecompressedImage->Save(filename.str().c_str());
                    cout << "[" << serialNumber << "] " << "Image saved at " << filename.str() << endl;
                }

                // Release image
                pResultImage->Release();

                cout << endl;
            }
            catch (Spinnaker::Exception& e)
            {
                cout << "[" << serialNumber << "] "
                    << "Error: " << e.what() << endl;
            }
        }

        // End acquisition
        pCam->EndAcquisition();

        // Free memory
        cudaFreeHost(pMemBuffersContiguous);
        cudaFreeHost(pHostOutputImageBuffer);

        cout << "[" << serialNumber << "] " << "Average decompression time cost : " << timeSum / k_numImages << endl;

#ifdef _DEBUG
        // Reset heartbeat for GEV camera
        ResetGVCPHeartbeat(pCam);
#endif

        // Deinitialize camera
        pCam->DeInit();

#if defined(_WIN32)
        return 1;
#else
        return (void*)1;
#endif
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
#if defined(_WIN32)
        return 0;
#else
        return (void*)0;
#endif
    }
}

// This function acts as the body of the example
int RunMultipleCameras(CameraList camList)
{
    int result = 0;
    unsigned int camListSize = 0;

    try
    {
        // Retrieve camera list size
        camListSize = camList.GetSize();

        // Create an array of CameraPtrs. This array maintenances smart pointer's reference
        // count when CameraPtr is passed into grab thread as void pointer

        // Create an array of handles
        CameraPtr* pCamList = new CameraPtr[camListSize];
#if defined(_WIN32)
        HANDLE* grabThreads = new HANDLE[camListSize];
#else
        pthread_t* grabThreads = new pthread_t[camListSize];
#endif

        for (unsigned int i = 0; i < camListSize; i++)
        {
            // Select camera
            pCamList[i] = camList.GetByIndex(i);
            // Start grab thread
#if defined(_WIN32)
            grabThreads[i] = CreateThread(nullptr, 0, AcquireImages, &pCamList[i], 0, nullptr);
            assert(grabThreads[i] != nullptr);
#else
            int err = pthread_create(&(grabThreads[i]), nullptr, &AcquireImages, &pCamList[i]);
            assert(err == 0);
#endif
        }

#if defined(_WIN32)
        // Wait for all threads to finish
        WaitForMultipleObjects(
            camListSize, // number of threads to wait for
            grabThreads, // handles for threads to wait for
            TRUE,        // wait for all of the threads
            INFINITE     // wait forever
        );

        // Check thread return code for each camera
        for (unsigned int i = 0; i < camListSize; i++)
        {
            DWORD exitcode;

            BOOL rc = GetExitCodeThread(grabThreads[i], &exitcode);
            if (!rc)
            {
                cout << "Handle error from GetExitCodeThread() returned for camera at index " << i << endl;
                result = -1;
            }
            else if (!exitcode)
            {
                cout << "Grab thread for camera at index " << i
                    << " exited with errors."
                    "Please check onscreen print outs for error details"
                    << endl;
                result = -1;
            }
        }

#else
        for (unsigned int i = 0; i < camListSize; i++)
        {
            // Wait for all threads to finish
            void* exitcode;
            int rc = pthread_join(grabThreads[i], &exitcode);
            if (rc != 0)
            {
                cout << "Handle error from pthread_join returned for camera at index " << i << endl;
                result = -1;
            }
            else if ((int)(intptr_t)exitcode == 0) // check thread return code for each camera
            {
                cout << "Grab thread for camera at index " << i
                    << " exited with errors."
                    "Please check onscreen print outs for error details"
                    << endl;
                result = -1;
            }
        }
#endif

        // Clear CameraPtr array and close all handles
        for (unsigned int i = 0; i < camListSize; i++)
        {
            pCamList[i] = 0;
#if defined(_WIN32)
            CloseHandle(grabThreads[i]);
#endif
        }

        // Delete array pointer
        delete[] pCamList;

        // Delete array pointer
        delete[] grabThreads;
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// Example entry point; please see Enumeration example for more in-depth
// comments on preparing and cleaning up the system.
int main(int /*argc*/, char** /*argv*/)
{
    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
    FILE* tempFile = fopen("test.txt", "w+");
    if (tempFile == nullptr)
    {
        cout << "Failed to create file in current folder.  Please check permissions." << endl;
        cout << "Press Enter to exit..." << endl;
        getchar();
        return -1;
    }

    fclose(tempFile);
    remove("test.txt");

    int result = 0;

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

    unsigned int numCameras = camList.GetSize();

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

    // Run example on all cameras
    cout << endl << "Running example for all cameras..." << endl;

    result = RunMultipleCameras(camList);

    cout << "Example complete..." << endl << endl;

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}