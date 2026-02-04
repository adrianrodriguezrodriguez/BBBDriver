#include "pch.h"
#include "BBBDriver.h"
#include <iostream>
#include <string>

static int AskInt(const char* msg, int defVal)
{
    std::cout << msg << " [" << defVal << "]: ";
    std::string s; std::getline(std::cin, s);
    if (s.empty()) return defVal;
    return std::stoi(s);
}

static float AskFloat(const char* msg, float defVal)
{
    std::cout << msg << " [" << defVal << "]: ";
    std::string s; std::getline(std::cin, s);
    if (s.empty()) return defVal;
    return std::stof(s);
}

static double AskDouble(const char* msg, double defVal)
{
    std::cout << msg << " [" << defVal << "]: ";
    std::string s; std::getline(std::cin, s);
    if (s.empty()) return defVal;
    return std::stod(s);
}

static void ReleaseImageList(Spinnaker::ImageList& set)
{
    for (unsigned int i = 0; i < set.GetSize(); i++)
    {
        Spinnaker::ImagePtr img = set.GetByIndex(i);
        if (img) img->Release();
    }
}

int main()
{
    BBBDriver driver;
    BBBParams p;

    std::cout << "BBBDriverConsole\n";
    std::cout << "1) Conecta la BBB\n";
    std::cout << "2) Ejecuta este programa\n\n";

    if (!driver.OpenFirstStereo())
    {
        std::cout << "No se detecta ninguna camara estereo BBB.\n";
        std::cout << "Pulsa Enter para salir.\n";
        std::string tmp; std::getline(std::cin, tmp);
        return 1;
    }

    if (!driver.ConfigureStreams_Rectified1_Disparity())
    {
        std::cout << "Fallo configurando streams Rectified1 + Disparity.\n";
        return 2;
    }

    if (!driver.ConfigureSoftwareTrigger())
    {
        std::cout << "Aviso: no pude activar trigger software.\n";
    }

    Scan3DParams s3d;
    if (!driver.ReadScan3DParams(s3d))
    {
        std::cout << "Fallo leyendo parametros Scan3D.\n";
        return 3;
    }

    while (true)
    {
        std::cout << "\nMENU\n";
        std::cout << "1) Capturar Disparity (PGM) + Rectified (PNG)\n";
        std::cout << "2) Generar nube 3D (PLY)\n";
        std::cout << "3) Distancias (centro y cara del bulto)\n";
        std::cout << "4) Cambiar parametros\n";
        std::cout << "5) Salir\n";
        std::cout << "Opcion: ";

        std::string opt;
        std::getline(std::cin, opt);

        if (opt == "5") break;

        if (opt == "4")
        {
            p.minRangeM = AskFloat("Min range (m)", p.minRangeM);
            p.maxRangeM = AskFloat("Max range (m)", p.maxRangeM);
            p.roiMinPct = AskInt("ROI min pct", p.roiMinPct);
            p.roiMaxPct = AskInt("ROI max pct", p.roiMaxPct);
            p.decimationFactor = AskInt("Decimation (1/2/4)", p.decimationFactor);

            double expUs = AskDouble("Exposure (us)", 5000.0);
            double gainDb = AskDouble("Gain (dB)", 0.0);

            driver.SetExposureUs(expUs);
            driver.SetGainDb(gainDb);

            std::cout << "OK parametros actualizados.\n";
            continue;
        }

        Spinnaker::ImageList set;
        if (!driver.CaptureOnceSync(set, 5000))
        {
            std::cout << "Fallo capturando set.\n";
            continue;
        }

        if (opt == "1")
        {
            driver.SaveDisparityPGM(set, "disparity.pgm");
            driver.SaveRectifiedPNG(set, "rectified.png");
            std::cout << "Guardado disparity.pgm / rectified.png\n";
        }
        else if (opt == "2")
        {
            if (driver.SavePointCloudPLY(set, s3d, p, "cloud.ply"))
                std::cout << "Guardado cloud.ply\n";
            else
                std::cout << "Fallo generando PLY.\n";
        }
        else if (opt == "3")
        {
            float zCenter = 0.f;
            float zBulto = 0.f;

            if (driver.GetDistanceCentralPointM(set, s3d, zCenter))
                std::cout << "Distancia punto central: " << zCenter << " m\n";
            else
                std::cout << "No pude calcular distancia punto central.\n";

            if (driver.GetDistanceToBultoM(set, s3d, p, zBulto))
                std::cout << "Distancia cara bulto (ROI + P10): " << zBulto << " m\n";
            else
                std::cout << "No pude calcular distancia a bulto.\n";
        }
        else
        {
            std::cout << "Opcion no valida.\n";
        }

        ReleaseImageList(set);
    }

    std::cout << "Saliendo...\n";
    return 0;
}
