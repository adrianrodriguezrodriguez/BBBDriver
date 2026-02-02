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
 *  @example Acquisition_CSharp.cs
 *
 *  Ejemplo de adquisición de imágenes con Spinnaker en C#.
 *   - Inicializa el sistema y la Bumblebee X.
 *   - Configura FPS, heartbeat, stream mode y parámetros 3D (Scan3d*).
 *   - Fuerza la cámara a modo DISPARITY + StereoResolution=Half.
 *   - Hace adquisición continua y calcula la distancia en el centro de la imagen.
 *   - Permite guardar un frame pulsando 'S' y salir con 'Q'.
 */

using System;
using System.IO;
using System.Collections.Generic;
using SpinnakerNET;
using SpinnakerNET.GenApi;

namespace Acquisition_CSharp
{
    class Program
    {
        // Enum interno para elegir el modo de streaming del driver
        enum StreamMode
        {
            STREAM_MODE_TELEDYNE_GIGE_VISION, // Modo GEV por defecto en SpinView (Windows)
            STREAM_MODE_PGRLWF,               // Driver ligero legacy (Windows)
                                              // Socket mode no está soportado en C#
        }

        // Modo de stream que usará este ejemplo
        StreamMode chosenStreamMode = StreamMode.STREAM_MODE_TELEDYNE_GIGE_VISION;

        // ==========================
        //  PARÁMETROS 3D BUMBLEBEE
        // ==========================
        // Estructura para agrupar todos los nodos Scan3d* que usa la fórmula 3D
        struct StereoParams
        {
            public float Baseline;         // Scan3dBaseline (m)
            public float FocalLength;      // Scan3dFocalLength (px)
            public float PrincipalPointU;  // Scan3dPrincipalPointU (px)
            public float PrincipalPointV;  // Scan3dPrincipalPointV (px)
            public float CoordScale;       // Scan3dCoordinateScale
            public float CoordOffset;      // Scan3dCoordinateOffset
            public bool HasInvalid;        // Scan3dInvalidDataFlag
            public float InvalidValue;     // Scan3dInvalidDataValue
        }

        // Lee los parámetros estereoscópicos desde el nodemap GenICam
        StereoParams ReadStereoParams(INodeMap nodeMap)
        {
            StereoParams p = new StereoParams();

            IFloat baselineNode = nodeMap.GetNode<IFloat>("Scan3dBaseline");
            IFloat focalNode = nodeMap.GetNode<IFloat>("Scan3dFocalLength");
            IFloat ppUNode = nodeMap.GetNode<IFloat>("Scan3dPrincipalPointU");
            IFloat ppVNode = nodeMap.GetNode<IFloat>("Scan3dPrincipalPointV");
            IFloat scaleNode = nodeMap.GetNode<IFloat>("Scan3dCoordinateScale");
            IFloat offsetNode = nodeMap.GetNode<IFloat>("Scan3dCoordinateOffset");
            IBool invalidFlag = nodeMap.GetNode<IBool>("Scan3dInvalidDataFlag");
            IFloat invalidValue = nodeMap.GetNode<IFloat>("Scan3dInvalidDataValue");

            if (baselineNode == null || focalNode == null || ppUNode == null ||
                ppVNode == null || scaleNode == null || offsetNode == null)
            {
                throw new SpinnakerException("No puedo leer todos los nodos Scan3d* necesarios.");
            }

            p.Baseline = (float)baselineNode.Value;
            p.FocalLength = (float)focalNode.Value;
            p.PrincipalPointU = (float)ppUNode.Value;
            p.PrincipalPointV = (float)ppVNode.Value;
            p.CoordScale = (float)scaleNode.Value;
            p.CoordOffset = (float)offsetNode.Value;

            p.HasInvalid = (invalidFlag != null && invalidFlag.IsReadable) ? invalidFlag.Value : false;
            p.InvalidValue = (invalidValue != null && invalidValue.IsReadable)
                                ? (float)invalidValue.Value
                                : 0f;

            Console.WriteLine("Stereo params:");
            Console.WriteLine("  Baseline        = {0} m", p.Baseline);
            Console.WriteLine("  FocalLength     = {0} px", p.FocalLength);
            Console.WriteLine("  PrincipalPointU = {0} px", p.PrincipalPointU);
            Console.WriteLine("  PrincipalPointV = {0} px", p.PrincipalPointV);
            Console.WriteLine("  CoordScale      = {0}", p.CoordScale);
            Console.WriteLine("  CoordOffset     = {0}", p.CoordOffset);
            Console.WriteLine("  HasInvalid      = {0}", p.HasInvalid);
            Console.WriteLine("  InvalidValue    = {0}", p.InvalidValue);
            Console.WriteLine();

            return p;
        }

        // Convierte el valor de disparidad de un píxel concreto a:
        //   zMeters  -> profundidad Z (metros)
        //   distMeters -> distancia euclídea cámara-punto (metros)
        bool TryGetDistanceAtPixel(
            IManagedImage disparityImage,
            StereoParams p,
            int x,
            int y,
            out float zMeters,
            out float distMeters)
        {
            zMeters = float.NaN;
            distMeters = float.NaN;

            int width = (int)disparityImage.Width;
            int height = (int)disparityImage.Height;

            // Comprobamos que el píxel está dentro de la imagen
            if (x < 0 || y < 0 || x >= width || y >= height)
                return false;

            // Cada píxel de disparidad es Mono16 -> 2 bytes por píxel
            byte[] data = disparityImage.ManagedData;
            int idx = (y * width + x) * 2;

            if (idx + 1 >= data.Length)
                return false;

            // Lectura little-endian -> ushort crudo
            ushort raw = (ushort)(data[idx] | (data[idx + 1] << 8));

            // Comprobamos valor inválido (si Scan3dInvalidDataFlag está activo)
            if (p.HasInvalid && Math.Abs(raw - p.InvalidValue) < 0.5f)
                return false;

            // Pasamos de valor RAW a disparidad física usando escala y offset
            float d = raw * p.CoordScale + p.CoordOffset;
            if (d <= 0.0f)
                return false;

            // Factor común de la reconstrucción 3D (especificación GenICam 3D)
            float factor = p.Baseline / d;

            // Reconstruimos X, Y, Z en metros (coordenadas cámara)
            float px = ((x + 0.5f) - p.PrincipalPointU) * factor;
            float py = ((y + 0.5f) - p.PrincipalPointV) * factor;
            float pz = p.FocalLength * factor;

            zMeters = pz;
            distMeters = (float)Math.Sqrt(px * px + py * py + pz * pz);

            return true;
        }

        // ====================================================================
        //  AJUSTE DE FPS (AcquisitionFrameRate)
        // ====================================================================
        // Configura el framerate de la cámara:
        //  - Apaga el modo auto si existe.
        //  - Activa AcquisitionFrameRateEnable si existe.
        //  - Escribe un valor de FPS dentro del rango permitido.
        int SetAcquisitionFrameRate(INodeMap nodeMap, double fps)
        {
            int result = 0;

            try
            {
                Console.WriteLine("\n*** CONFIGURANDO FPS DE CÁMARA ***");

                // 1) Desactivar AUTO si existe
                IEnum acqFrameRateAuto = nodeMap.GetNode<IEnum>("AcquisitionFrameRateAuto");
                if (acqFrameRateAuto != null && acqFrameRateAuto.IsWritable && acqFrameRateAuto.IsReadable)
                {
                    IEnumEntry offEntry = acqFrameRateAuto.GetEntryByName("Off");
                    if (offEntry != null && offEntry.IsReadable)
                    {
                        acqFrameRateAuto.Value = offEntry.Symbolic;
                        Console.WriteLine("AcquisitionFrameRateAuto = Off");
                    }
                }

                // 2) Activar el booleano de enable si existe
                IBool acqFrameRateEnable = nodeMap.GetNode<IBool>("AcquisitionFrameRateEnable");
                if (acqFrameRateEnable != null && acqFrameRateEnable.IsWritable)
                {
                    acqFrameRateEnable.Value = true;
                    Console.WriteLine("AcquisitionFrameRateEnable = true");
                }

                // 3) Buscar el nodo de frame rate (AcquisitionFrameRate o FrameRate)
                IFloat acqFrameRate = nodeMap.GetNode<IFloat>("AcquisitionFrameRate");
                if (acqFrameRate == null || !acqFrameRate.IsReadable || !acqFrameRate.IsWritable)
                {
                    // Algunos modelos lo llaman simplemente "FrameRate"
                    acqFrameRate = nodeMap.GetNode<IFloat>("FrameRate");
                }

                if (acqFrameRate == null || !acqFrameRate.IsReadable || !acqFrameRate.IsWritable)
                {
                    Console.WriteLine("No encuentro nodo de frame rate accesible (AcquisitionFrameRate / FrameRate).");
                    return -1;
                }

                double min = acqFrameRate.Min;
                double max = acqFrameRate.Max;

                // Se pide un valor y se clampéa al rango permitido
                double fpsRequested = fps;
                double fpsClamped = Math.Max(min, Math.Min(max, fpsRequested));

                if (Math.Abs(fpsClamped - fpsRequested) > 1e-6)
                {
                    Console.WriteLine(
                        "La cámara NO permite {0} fps; ajustando a rango [{1}, {2}] -> {3} fps",
                        fpsRequested, min, max, fpsClamped);
                }

                acqFrameRate.Value = fpsClamped;

                Console.WriteLine("AcquisitionFrameRate configurado a {0:0.###} fps (rango permitido [{1:0.###}, {2:0.###}])\n",
                    acqFrameRate.Value, min, max);
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error al configurar FPS: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // ====================================================================
        //  HEARTBEAT GEV
        // ====================================================================
        // Activa o desactiva el heartbeat (solo GigE Vision).
        // Muy útil para debug: evitas timeouts mientras el programa está parado.
        static int ConfigureGVCPHeartbeat(IManagedCamera cam, bool enableHeartbeat)
        {
            // Nodemap del transporte (TL)
            INodeMap nodeMapTLDevice = cam.GetTLDeviceNodeMap();

            // Nodemap GenICam de control
            INodeMap nodeMap = cam.GetNodeMap();

            IEnum iDeviceType = nodeMapTLDevice.GetNode<IEnum>("DeviceType");
            IEnumEntry iDeviceTypeGEV = iDeviceType.GetEntryByName("GigEVision");

            // Confirmamos que es cámara GigE Vision
            if (iDeviceType != null && iDeviceType.IsReadable)
            {
                if (iDeviceType.Value == iDeviceTypeGEV.Value)
                {
                    if (enableHeartbeat)
                    {
                        Console.WriteLine("Resetting heartbeat");
                    }
                    else
                    {
                        Console.WriteLine("Disabling heartbeat");
                    }

                    IBool iGEVHeartbeatDisable = nodeMap.GetNode<IBool>("GevGVCPHeartbeatDisable");
                    if (iGEVHeartbeatDisable == null || !iGEVHeartbeatDisable.IsWritable)
                    {
                        Console.WriteLine(
                            "Unable to disable heartbeat on camera. Continuing with execution as this may be non-fatal...");
                    }
                    else
                    {
                        // Este nodo es "Disable": true = deshabilitado
                        iGEVHeartbeatDisable.Value = !enableHeartbeat;

                        if (!enableHeartbeat)
                        {
                            Console.WriteLine("WARNING: Heartbeat has been disabled for the rest of this example run.");
                            Console.WriteLine(
                                "         Heartbeat will be reset upon the completion of this run.  If the ");
                            Console.WriteLine(
                                "         example is aborted unexpectedly before the heartbeat is reset, the");
                            Console.WriteLine("         camera may need to be power cycled to reset the heartbeat.\n");
                        }
                        else
                        {
                            Console.WriteLine("Heartbeat has been reset.\n");
                        }
                        Console.WriteLine();
                    }
                }
            }
            else
            {
                Console.WriteLine("Unable to access TL device nodemap. Aborting...");
                return -1;
            }

            return 0;
        }

        static int ResetGVCPHeartbeat(IManagedCamera cam)
        {
            return ConfigureGVCPHeartbeat(cam, true);
        }

        static int DisableGVCPHeartbeat(IManagedCamera cam)
        {
            return ConfigureGVCPHeartbeat(cam, false);
        }

        // ====================================================================
        //  STREAM MODE
        // ====================================================================
        // Configura el modo de streaming del TLStream (TeledyneGigeVision / LWF)
        int SetStreamMode(IManagedCamera cam)
        {
            // Nodemap del stream
            INodeMap sNodeMap = cam.GetTLStreamNodeMap();

            IEnum iStreamMode = sNodeMap.GetNode<IEnum>("StreamMode");
            if (iStreamMode == null || !iStreamMode.IsWritable || !iStreamMode.IsReadable)
            {
                // Si el nodo no existe, salimos sin error
                return 0;
            }

            string streamMode;
            switch (chosenStreamMode)
            {
                case StreamMode.STREAM_MODE_PGRLWF:
                    streamMode = "LWF";
                    break;
                case StreamMode.STREAM_MODE_TELEDYNE_GIGE_VISION:
                default:
                    streamMode = "TeledyneGigeVision";
                    break;
            }

            IEnumEntry iStreamModeCustom = iStreamMode.GetEntryByName(streamMode);
            if (iStreamModeCustom == null || !iStreamModeCustom.IsReadable)
            {
                Console.WriteLine("Custom stream mode is not available...");
                return -1;
            }

            iStreamMode.Value = iStreamModeCustom.Value;
            Console.WriteLine("Stream Mode set to {0}...", iStreamMode.Value.String);

            return 0;
        }

        // ====================================================================
        //  CONFIGURACIÓN BUMBLEBEE: DISPARITY + STEREO HALF
        // ====================================================================
        // Activa el componente de disparidad del Sensor1 y desactiva el rectificado.
        void SetBumblebeeModeDisparity(INodeMap nodeMap)
        {
            IEnum sourceSelector = nodeMap.GetNode<IEnum>("SourceSelector");
            IEnum componentSelector = nodeMap.GetNode<IEnum>("ComponentSelector");
            IBool componentEnable = nodeMap.GetNode<IBool>("ComponentEnable");

            if (sourceSelector == null || componentSelector == null || componentEnable == null ||
                !sourceSelector.IsReadable || !sourceSelector.IsWritable ||
                !componentSelector.IsReadable || !componentSelector.IsWritable ||
                !componentEnable.IsReadable || !componentEnable.IsWritable)
            {
                Console.WriteLine("No puedo acceder a SourceSelector/ComponentSelector/ComponentEnable.");
                return;
            }

            IEnumEntry sourceSensor1 = sourceSelector.GetEntryByName("Sensor1");
            IEnumEntry componentRectified = componentSelector.GetEntryByName("Rectified");
            IEnumEntry componentDisparity = componentSelector.GetEntryByName("Disparity");

            if (sourceSensor1 == null || !sourceSensor1.IsReadable ||
                componentRectified == null || !componentRectified.IsReadable ||
                componentDisparity == null || !componentDisparity.IsReadable)
            {
                Console.WriteLine("No puedo leer entradas Sensor1/Rectified/Disparity.");
                return;
            }

            // Desactivar RECTIFIED
            sourceSelector.Value = sourceSensor1.Symbolic;
            componentSelector.Value = componentRectified.Symbolic;
            componentEnable.Value = false;

            // Activar DISPARITY
            sourceSelector.Value = sourceSensor1.Symbolic;
            componentSelector.Value = componentDisparity.Symbolic;
            componentEnable.Value = true;

            Console.WriteLine("Modo BUMBLEBEE: DISPARITY activado, RECTIFIED desactivado.");
        }

        // Pone StereoResolution = Half (equivalente al desplegable Full/Half del SpinView)
        void SetStereoResolutionHalf(INodeMap nodeMap)
        {
            try
            {
                IEnum stereoRes = nodeMap.GetNode<IEnum>("StereoResolution");
                if (stereoRes == null || !stereoRes.IsReadable || !stereoRes.IsWritable)
                {
                    Console.WriteLine("No puedo acceder al nodo StereoResolution (Full/Half).");
                    return;
                }

                IEnumEntry halfEntry = stereoRes.GetEntryByName("Half");
                if (halfEntry == null || !halfEntry.IsReadable)
                {
                    Console.WriteLine("El modo 'Half' no está disponible en StereoResolution.");
                    return;
                }

                stereoRes.Value = halfEntry.Symbolic;
                Console.WriteLine("StereoResolution = {0}", stereoRes.Value.String);
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error al configurar StereoResolution=Half: {0}", ex.Message);
            }
        }

        // ====================================================================
        //  ADQUISICIÓN INTERACTIVA (DISPARITY + DISTANCIAS)
        // ====================================================================
        // Bucle principal de adquisición:
        //   - Pone modo Continuous.
        //   - Fuerza DISPARITY + StereoResolution=Half.
        //   - Captura frames continuamente.
        //   - Para cada frame:
        //       * Loguea tamaño y formato.
        //       * Calcula distancia en el centro de la imagen.
        //       * Si se pulsa 'S', guarda el frame como PNG (Mono16).
        //       * Si se pulsa 'Q', sale del bucle.
        int AcquireImagesInteractiveBumblebee(
            IManagedCamera cam,
            INodeMap nodeMap,
            INodeMap nodeMapTLDevice)
        {
            int result = 0;

            Console.WriteLine("\n*** IMAGE ACQUISITION (BUMBLEBEE INTERACTIVO DISPARITY) ***\n");

            try
            {
                // Cargar parámetros estereo para poder reconstruir Z/distancias
                StereoParams stereoParams = ReadStereoParams(nodeMap);

                // Poner AcquisitionMode = Continuous
                IEnum iAcquisitionMode = nodeMap.GetNode<IEnum>("AcquisitionMode");
                if (iAcquisitionMode == null || !iAcquisitionMode.IsWritable)
                {
                    Console.WriteLine("No puedo poner AcquisitionMode=Continuous.");
                    return -1;
                }

                IEnumEntry iAcquisitionModeContinuous = iAcquisitionMode.GetEntryByName("Continuous");
                if (iAcquisitionModeContinuous == null || !iAcquisitionModeContinuous.IsReadable)
                {
                    Console.WriteLine("No puedo leer el valor 'Continuous' de AcquisitionMode.");
                    return -1;
                }

                iAcquisitionMode.Value = iAcquisitionModeContinuous.Symbolic;
                Console.WriteLine("Acquisition mode = Continuous.\n");

                // Configuración inicial: DISPARITY + StereoResolution=Half
                SetBumblebeeModeDisparity(nodeMap);
                SetStereoResolutionHalf(nodeMap);

                Console.WriteLine("Modo inicial fijo: DISPARITY (Sensor1, StereoResolution=Half).");

                // Comenzar adquisición
                cam.BeginAcquisition();
                Console.WriteLine("Capturando imágenes...\n");
                Console.WriteLine("Controles:");
                Console.WriteLine("  S -> Guardar frame actual");
                Console.WriteLine("  Q -> Salir\n");

                // Número de serie para nombre de archivos
                string deviceSerialNumber = "";
                IString iDeviceSerialNumber = nodeMapTLDevice.GetNode<IString>("DeviceSerialNumber");
                if (iDeviceSerialNumber != null && iDeviceSerialNumber.IsReadable)
                {
                    deviceSerialNumber = iDeviceSerialNumber.Value;
                    Console.WriteLine("Device serial: {0}\n", deviceSerialNumber);
                }

                // Procesador de imágenes (solo lo usamos para guardar como Mono16 PNG)
                IManagedImageProcessor processor = new ManagedImageProcessor();
                processor.SetColorProcessing(ColorProcessingAlgorithm.HQ_LINEAR);

                int frameId = 0;
                bool saveNextFrame = false;

                while (true)
                {
                    // Gestión de teclado no bloqueante
                    if (Console.KeyAvailable)
                    {
                        var keyInfo = Console.ReadKey(true);

                        if (keyInfo.Key == ConsoleKey.S)
                        {
                            saveNextFrame = true;
                        }
                        else if (keyInfo.Key == ConsoleKey.Q)
                        {
                            Console.WriteLine("Saliendo del bucle de adquisición...");
                            break;
                        }
                    }

                    // Obtener siguiente imagen de disparidad
                    using (IManagedImage rawImage = cam.GetNextImage(1000))
                    {
                        if (rawImage.IsIncomplete)
                        {
                            Console.WriteLine("[{0}] Imagen incompleta. Status={1}",
                                frameId, rawImage.ImageStatus);
                            continue;
                        }

                        Console.WriteLine(
                            "[{0}] DISPARITY  Size={1}x{2}  RawPixFmt={3}",
                            frameId,
                            rawImage.Width,
                            rawImage.Height,
                            rawImage.PixelFormatName);

                        // Coordenadas del centro de la imagen
                        int cx = (int)rawImage.Width / 2;
                        int cy = (int)rawImage.Height / 2;

                        // Calculamos Z y distancia en el centro usando el mapa de disparidad
                        if (TryGetDistanceAtPixel(
                                rawImage,
                                stereoParams,
                                cx,
                                cy,
                                out float zMeters,
                                out float distMeters))
                        {
                            Console.WriteLine("   Centro ({0},{1}): Z = {2:0.###} m  Dist = {3:0.###} m",
                                cx, cy, zMeters, distMeters);
                        }
                        else
                        {
                            Console.WriteLine("   Centro ({0},{1}): valor inválido (disparidad inválida)",
                                cx, cy);
                        }

                        // Guardar frame actual si se ha pedido con 'S'
                        if (saveNextFrame)
                        {
                            string filename = $"Grab-DISPARITY-{deviceSerialNumber}-{frameId}.png";

                            try
                            {
                                using (IManagedImage converted =
                                    processor.Convert(rawImage, PixelFormatEnums.Mono16))
                                {
                                    converted.Save(filename);
                                }

                                Console.WriteLine("   >> Frame guardado en {0}", filename);
                            }
                            catch (SpinnakerException ex)
                            {
                                Console.WriteLine("   Error al guardar frame: {0}", ex.Message);
                            }

                            saveNextFrame = false;
                        }

                        frameId++;
                    }
                }

                cam.EndAcquisition();
                Console.WriteLine("Adquisición detenida.\n");
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error en AcquireImagesInteractiveBumblebee: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // ====================================================================
        //  EJEMPLO ORIGINAL (AcquireImages) – NO LO USAS AHORA MISMO
        // ====================================================================
        // Versión estándar del ejemplo de FLIR (captura 10 imágenes Mono8 y las guarda).
        static int AcquireImages(IManagedCamera cam, INodeMap nodeMap, INodeMap nodeMapTLDevice)
        {
            int result = 0;

            Console.WriteLine("\n*** IMAGE ACQUISITION ***\n");

            try
            {
                IEnum iAcquisitionMode = nodeMap.GetNode<IEnum>("AcquisitionMode");
                if (iAcquisitionMode == null || !iAcquisitionMode.IsWritable || !iAcquisitionMode.IsReadable)
                {
                    Console.WriteLine("Unable to set acquisition mode to continuous (node retrieval). Aborting...\n");
                    return -1;
                }

                IEnumEntry iAcquisitionModeContinuous = iAcquisitionMode.GetEntryByName("Continuous");
                if (iAcquisitionModeContinuous == null || !iAcquisitionModeContinuous.IsReadable)
                {
                    Console.WriteLine(
                        "Unable to set acquisition mode to continuous (enum entry retrieval). Aborting...\n");
                    return -1;
                }

                iAcquisitionMode.Value = iAcquisitionModeContinuous.Symbolic;
                Console.WriteLine("Acquisition mode set to continuous...");

                cam.BeginAcquisition();
                Console.WriteLine("Acquiring images...");

                string deviceSerialNumber = "";

                IString iDeviceSerialNumber = nodeMapTLDevice.GetNode<IString>("DeviceSerialNumber");
                if (iDeviceSerialNumber != null && iDeviceSerialNumber.IsReadable)
                {
                    deviceSerialNumber = iDeviceSerialNumber.Value;
                    Console.WriteLine("Device serial number retrieved as {0}...", deviceSerialNumber);
                }
                Console.WriteLine();

                const int NumImages = 10;
                IManagedImageProcessor processor = new ManagedImageProcessor();
                processor.SetColorProcessing(ColorProcessingAlgorithm.HQ_LINEAR);

                for (int imageCnt = 0; imageCnt < NumImages; imageCnt++)
                {
                    try
                    {
                        using (IManagedImage rawImage = cam.GetNextImage(1000))
                        {
                            if (rawImage.IsIncomplete)
                            {
                                Console.WriteLine("Image incomplete with image status {0}...", rawImage.ImageStatus);
                            }
                            else
                            {
                                uint width = rawImage.Width;
                                uint height = rawImage.Height;

                                Console.WriteLine(
                                    "Grabbed image {0}, width = {1}, height = {2}", imageCnt, width, height);

                                using (IManagedImage convertedImage =
                                    processor.Convert(rawImage, PixelFormatEnums.Mono8))
                                {
                                    string filename = "Acquisition-CSharp-";
                                    if (deviceSerialNumber != "")
                                    {
                                        filename = filename + deviceSerialNumber + "-";
                                    }
                                    filename = filename + imageCnt + ".jpg";

                                    convertedImage.Save(filename);
                                    Console.WriteLine("Image saved at {0}\n", filename);
                                }
                            }
                        }
                    }
                    catch (SpinnakerException ex)
                    {
                        Console.WriteLine("Error: {0}", ex.Message);
                        result = -1;
                    }
                }

                cam.EndAcquisition();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // ====================================================================
        //  PRINT DEVICE INFO
        // ====================================================================
        // Imprime por consola la info de DeviceInformation del TL nodemap.
        static int PrintDeviceInfo(INodeMap nodeMap)
        {
            int result = 0;

            try
            {
                Console.WriteLine("\n*** DEVICE INFORMATION ***\n");

                ICategory category = nodeMap.GetNode<ICategory>("DeviceInformation");
                if (category != null && category.IsReadable)
                {
                    for (int i = 0; i < category.Children.Length; i++)
                    {
                        Console.WriteLine(
                            "{0}: {1}",
                            category.Children[i].Name,
                            (category.Children[i].IsReadable ? category.Children[i].ToString()
                             : "Node not available"));
                    }
                    Console.WriteLine();
                }
                else
                {
                    Console.WriteLine("Device control information not available.");
                }
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // ====================================================================
        //  BODY DEL EJEMPLO (POR CADA CÁMARA)
        // ====================================================================
        // Secuencia:
        //   - Saca DeviceInfo.
        //   - Init cámara.
        //   - Configura FPS al máximo permitido.
        //   - Configura heartbeat + stream mode.
        //   - Lanza el bucle de adquisición interactivo con distancias.
        //   - Resetea heartbeat y deinit.
        int RunSingleCamera(IManagedCamera cam)
        {
            int result = 0;

            try
            {
                // TL device nodemap + info
                INodeMap nodeMapTLDevice = cam.GetTLDeviceNodeMap();
                result |= PrintDeviceInfo(nodeMapTLDevice);

                // Inicializar cámara
                cam.Init();

                // Nodemap GenICam principal
                INodeMap nodeMap = cam.GetNodeMap();

                // FPS al máximo que permita: pedimos 10000 y se clampéa a Max
                result |= SetAcquisitionFrameRate(nodeMap, 10000.0);

                // Heartbeat: deshabilitado en DEBUG, reseteado en RELEASE
#if DEBUG
                result |= DisableGVCPHeartbeat(cam);
#else
                result |= ResetGVCPHeartbeat(cam);
#endif

                // Stream mode (si aplica)
                result |= SetStreamMode(cam);

                // Bucle interactivo disparity + distancias
                result |= AcquireImagesInteractiveBumblebee(cam, nodeMap, nodeMapTLDevice);

#if DEBUG
                // Reset heartbeat para dejar la cámara limpia al salir
                result |= ResetGVCPHeartbeat(cam);
#endif

                // Desinicializar cámara
                cam.DeInit();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // ====================================================================
        //  MAIN
        // ====================================================================
        // Punto de entrada:
        //   - Comprueba permisos de escritura.
        //   - Crea el ManagedSystem y lista cámaras.
        //   - Ejecuta RunSingleCamera sobre cada cámara encontrada.
        static int Main(string[] args)
        {
            int result = 0;

            Program program = new Program();

            // Comprobar que podemos escribir en la carpeta actual
            FileStream fileStream;
            try
            {
                fileStream = new FileStream(@"test.txt", FileMode.Create);
                fileStream.Close();
                File.Delete("test.txt");
            }
            catch
            {
                Console.WriteLine("Failed to create file in current folder. Please check permissions.");
                Console.WriteLine("Press enter to exit...");
                Console.ReadLine();
                return -1;
            }

            // Objeto sistema de Spinnaker
            ManagedSystem system = new ManagedSystem();

            // Versión de la librería
            LibraryVersion spinVersion = system.GetLibraryVersion();
            Console.WriteLine(
                "Spinnaker library version: {0}.{1}.{2}.{3}\n\n",
                spinVersion.major,
                spinVersion.minor,
                spinVersion.type,
                spinVersion.build);

            // Lista de cámaras detectadas
            ManagedCameraList camList = system.GetCameras();
            Console.WriteLine("Number of cameras detected: {0}\n\n", camList.Count);

            if (camList.Count == 0)
            {
                camList.Clear();
                system.Dispose();

                Console.WriteLine("Not enough cameras!");
                Console.WriteLine("Done! Press Enter to exit...");
                Console.ReadLine();
                return -1;
            }

            int index = 0;

            // Ejecutar el ejemplo sobre cada cámara
            foreach (IManagedCamera managedCamera in camList) using (managedCamera)
                {
                    Console.WriteLine("Running example for camera {0}...", index);

                    try
                    {
                        result |= program.RunSingleCamera(managedCamera);
                    }
                    catch (SpinnakerException ex)
                    {
                        Console.WriteLine("Error: {0}", ex.Message);
                        result = -1;
                    }

                    Console.WriteLine("Camera {0} example complete...\n", index++);
                }

            // Limpiar lista y liberar sistema
            camList.Clear();
            system.Dispose();

            Console.WriteLine("\nDone! Press Enter to exit...");
            Console.ReadLine();

            return result;
        }
    }
}
