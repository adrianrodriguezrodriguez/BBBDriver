#include "BBBDriver.h"
#include "BBBConfig.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <ctime>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <utility>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

static std::filesystem::path GetExePath()
{
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0) return std::filesystem::path();
    return std::filesystem::path(std::string(buf, buf + len));
#else
    return std::filesystem::current_path() / "BBBDriverConsole";
#endif
}

static std::filesystem::path GetExeDir()
{
    auto p = GetExePath();
    if (p.empty()) return std::filesystem::current_path();
    return p.parent_path();
}

static std::filesystem::path FindIniPath(const std::string& iniName)
{
    std::error_code ec;

    auto pCwd = std::filesystem::current_path(ec) / iniName;
    if (!ec && std::filesystem::exists(pCwd, ec) && !ec) return pCwd;

    auto exeDir = GetExeDir();
    auto pExe = exeDir / iniName;
    if (std::filesystem::exists(pExe, ec) && !ec) return pExe;

    return pExe;
}

static std::string NowTag()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    tm = *std::localtime(&t);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

static void ReleaseImageList(Spinnaker::ImageList& set)
{
    const unsigned int n = (unsigned int)set.GetSize();
    for (unsigned int i = 0; i < n; i++)
    {
        Spinnaker::ImagePtr img = set.GetByIndex(i);
        if (img) img->Release();
    }
}

static void EnsureBaseDir(const BBBPaths& paths)
{
    std::filesystem::path base(paths.outputDir);
    std::filesystem::create_directories(base);
}

static std::string SanitizeFileTag(std::string s)
{
    for (auto& c : s)
    {
        bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            (c == '_') || (c == '-');
        if (!ok) c = '_';
    }
    if (s.empty()) s = "BBB";
    return s;
}

static std::string ToLowerSimple(std::string s)
{
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string TrimSimple(std::string s)
{
    auto isspace2 = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isspace2((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace2((unsigned char)s.back())) s.pop_back();
    return s;
}

static std::string NormalizeOrient(std::string s)
{
    s = ToLowerSimple(TrimSimple(s));

    if (s == "izq" || s == "izquierda" || s == "left") return "izq";
    if (s == "der" || s == "derecha" || s == "right") return "der";
    if (s == "cen" || s == "cenital" || s == "top") return "cenital";

    if (s.empty()) return "";
    return SanitizeFileTag(s);
}

// ARR construimos prefijo BBB + serial + orientacion
static std::string MakeCamPrefix(const BBBAppConfig& cfg, const CameraConfig& c, int index0Based)
{
    std::string baseName;

    // ARR si hay serial siempre lo usamos
    if (!c.serial.empty())
        baseName = cfg.namePrefix + c.serial;
    else if (!c.name.empty())
        baseName = c.name;
    else
        baseName = cfg.namePrefix + std::string("UNASSIGNED") + std::to_string(index0Based + 1);

    std::string o = NormalizeOrient(c.orient);
    if (o.empty())
    {
        if (index0Based == 0) o = "izq";
        else if (index0Based == 1) o = "der";
        else o = "cenital";
    }

    return SanitizeFileTag(baseName + "_" + o);
}

// ARR estructura final
// ARR outputDir/BBBserial_orient/PNG ...
// ARR outputDir/BBBserial_orient/PGM ...
// ARR outputDir/BBBserial_orient/PLY ...
static void EnsureCamDirs(const BBBAppConfig& cfg)
{
    std::filesystem::path base(cfg.paths.outputDir);

    for (int i = 0; i < (int)cfg.cameras.size(); ++i)
    {
        const auto& c = cfg.cameras[i];
        std::string prefix = MakeCamPrefix(cfg, c, i);

        std::filesystem::path camBase = base / prefix;
        std::filesystem::create_directories(camBase / cfg.paths.dirPNG);
        std::filesystem::create_directories(camBase / cfg.paths.dirPGM);
        std::filesystem::create_directories(camBase / cfg.paths.dirPLY);
    }
}

static void ApplyControl(BBBDriver& d, const BBBControl& c)
{
    d.SetExposureUs(c.exposureUs);
    d.SetGainDb(c.gainDb);
}

static void PrintMenu()
{
    std::cout << "\n---------------------------------\n";
    std::cout << "MENU\n";
    std::cout << " 1 Guardar Disparity (disparidad) PGM y Rectified (rectificada) PNG\n";
    std::cout << " 2 Generar PLY (archivo de nube) filtrado\n";
    std::cout << " 3 Medir distancia\n";
    std::cout << " 4 Cambiar parametros\n";
    std::cout << " 5 Releer Scan3D\n";
    std::cout << " 0 Salir\n";
    std::cout << "Opcion: ";
}

struct ActiveCam
{
    CameraConfig* cfg = nullptr;
    BBBDriver drv;
    Scan3DParams s3d{};
    bool available = false;
};

static std::vector<std::string> DetectStereoSerials(Spinnaker::CameraList& cams)
{
    std::vector<std::string> out;

    for (unsigned int i = 0; i < cams.GetSize(); ++i)
    {
        Spinnaker::CameraPtr c = cams.GetByIndex(i);
        c->Init();

        bool isStereo = Spinnaker::ImageUtilityStereo::IsStereoCamera(c);
        std::string serial = c->TLDevice.DeviceSerialNumber.ToString().c_str();

        c->DeInit();

        if (isStereo && !serial.empty())
            out.push_back(serial);
    }

    // ARR quitamos duplicados por seguridad
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());

    return out;
}

int main()
{
    std::cout << "=== BBBDriverConsole BBB Spinnaker hasta 3 camaras ===\n";
    std::cout << "Guardado por camara en outputDir/BBBserial_orient/PNG PGM PLY\n\n";

    const std::string iniName = "bbb_config.ini";

    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    auto exeDir = GetExeDir();

    std::cout << "Directorio actual " << (ec ? std::string("desconocido") : cwd.string()) << "\n";
    std::cout << "Directorio exe " << exeDir.string() << "\n";

    std::filesystem::path iniPath = FindIniPath(iniName);
    std::cout << "Buscando INI en " << iniPath.string() << "\n";

    BBBAppConfig cfg;
    if (!BBBConfig::LoadIni(iniPath.string(), cfg))
    {
        std::cout << "INI no existe o no se pudo leer, lo creo en " << iniPath.string() << "\n";
        BBBConfig::SaveIni(iniPath.string(), cfg);
    }

    if (cfg.maxCameras > 3) cfg.maxCameras = 3;
    if (cfg.maxCameras < 1) cfg.maxCameras = 1;

    if (cfg.paths.outputDir.empty() || cfg.paths.outputDir == ".")
        cfg.paths.outputDir = exeDir.string();

    EnsureBaseDir(cfg.paths);

    Spinnaker::SystemPtr system = Spinnaker::System::GetInstance();
    Spinnaker::CameraList cams = system->GetCameras();

    if (cams.GetSize() == 0)
    {
        std::cout << "ERROR no hay camaras detectadas\n";
        system->ReleaseInstance();
        return 2;
    }

    std::vector<std::string> detected = DetectStereoSerials(cams);
    if (detected.empty())
    {
        std::cout << "ERROR no hay camaras estereo detectadas\n";
        cams.Clear();
        system->ReleaseInstance();
        return 3;
    }

    bool cfgChanged = false;
    BBBConfig::EnsureDetectedCameras(cfg, detected, cfgChanged);

    if (cfgChanged)
    {
        BBBConfig::SaveIni(iniPath.string(), cfg);
        std::cout << "INI actualizado al detectar camaras\n";
    }

    EnsureCamDirs(cfg);

    // ARR abrimos cada Camera.0..2 una vez sin serial duplicado
    std::vector<ActiveCam> act;
    act.reserve((size_t)cfg.maxCameras);

    std::vector<std::string> usedSerials;

    auto IsUsed = [&](const std::string& s)
        {
            return std::find(usedSerials.begin(), usedSerials.end(), s) != usedSerials.end();
        };

    for (int i = 0; i < cfg.maxCameras; ++i)
    {
        CameraConfig& c = cfg.cameras[i];

        if (!c.enabled)
            continue;

        if (c.serial.empty())
        {
            if (c.name.empty() && cfg.autoNameFromSerial)
                c.name = BBBConfig::MakeAutoName(cfg, "", i + 1);

            ActiveCam a;
            a.cfg = &c;
            a.available = false;
            act.push_back(std::move(a));
            continue;
        }

        if (IsUsed(c.serial))
        {
            std::cout << "AVISO serial duplicado en INI " << c.serial << " en " << c.name << " lo saltamos\n";
            continue;
        }

        if (c.name.empty() && cfg.autoNameFromSerial)
            c.name = BBBConfig::MakeAutoName(cfg, c.serial, i + 1);

        ActiveCam a;
        a.cfg = &c;
        a.available = a.drv.OpenBySerial(cams, c.serial);

        if (a.available)
            usedSerials.push_back(c.serial);

        act.push_back(std::move(a));
    }

    BBBConfig::SaveIni(iniPath.string(), cfg);

    for (auto& a : act)
    {
        if (!a.cfg) continue;

        std::cout << "Camara " << a.cfg->name << " serial " << (a.cfg->serial.empty() ? "SIN_SERIAL" : a.cfg->serial)
            << " " << (a.available ? "OK" : "NO") << "\n";

        if (!a.available) continue;

#ifdef _DEBUG
        a.drv.DisableGVCPHeartbeat(true);
#endif

        if (!a.drv.ConfigureStreams_Rectified1_Disparity())
            std::cout << "AVISO " << a.cfg->name << " no pudo configurar streams\n";

        if (!a.drv.ConfigureSoftwareTrigger())
            std::cout << "AVISO " << a.cfg->name << " no pudo configurar trigger software\n";

        if (!a.drv.ReadScan3DParams(a.s3d))
            std::cout << "AVISO " << a.cfg->name << " no pudo leer Scan3D\n";
        else
            std::cout << a.cfg->name << " Scan3D baseline " << a.s3d.baseline
            << " focal " << a.s3d.focal
            << " scale " << a.s3d.scale
            << " offset " << a.s3d.offset << "\n";

        ApplyControl(a.drv, a.cfg->control);

        if (!a.drv.StartAcquisition())
        {
            std::cout << "AVISO " << a.cfg->name << " no pudo iniciar adquisicion\n";
            a.available = false;
        }
    }

    while (true)
    {
        PrintMenu();
        std::string opt;
        std::getline(std::cin, opt);

        if (opt == "0") break;

        const std::string tag = NowTag();
        std::filesystem::path base(cfg.paths.outputDir);

        if (opt == "5")
        {
            std::cout << "Releyendo Scan3D (baseline linea base, focal, scale escala, offset desfase)\n";
            for (auto& a : act)
            {
                if (!a.available) continue;

                if (a.drv.ReadScan3DParams(a.s3d))
                    std::cout << a.cfg->name << " baseline " << a.s3d.baseline
                    << " focal " << a.s3d.focal
                    << " scale " << a.s3d.scale
                    << " offset " << a.s3d.offset << "\n";
                else
                    std::cout << a.cfg->name << " FAIL Scan3D\n";
            }
            continue;
        }

        if (opt == "4")
        {
            std::cout << "\nElegir camara para cambiar parametros\n";
            for (size_t i = 0; i < act.size(); ++i)
            {
                auto& a = act[i];
                std::cout << " " << (i + 1) << " " << a.cfg->name
                    << " serial " << (a.cfg->serial.empty() ? "SIN_SERIAL" : a.cfg->serial)
                    << " " << (a.available ? "OK" : "NO") << "\n";
            }
            std::cout << "Opcion: ";
            std::string sel;
            std::getline(std::cin, sel);

            int idx = std::stoi(sel) - 1;
            if (idx < 0 || idx >= (int)act.size())
            {
                std::cout << "Opcion no valida\n";
                continue;
            }

            std::cout << "Editando parametros de " << act[idx].cfg->name << " en INI\n";
            std::cout << "Hacemos los cambios editando el bbb_config.ini\n";

            BBBConfig::SaveIni(iniPath.string(), cfg);
            continue;
        }

        auto DoCam = [&](ActiveCam& a)
            {
                if (!a.available) return;

                Spinnaker::ImageList set;
                if (!a.drv.CaptureOnceSync(set, cfg.paths.captureTimeoutMs))
                {
                    std::cout << a.cfg->name << " FAIL no capturamos set\n";
                    ReleaseImageList(set);
                    return;
                }

                int camIndex = (int)(a.cfg - cfg.cameras.data());
                if (camIndex < 0 || camIndex >= (int)cfg.cameras.size()) camIndex = 0;

                std::string camPrefix = MakeCamPrefix(cfg, *a.cfg, camIndex);

                std::filesystem::path camBase = base / camPrefix;
                std::filesystem::path camDirPNG = camBase / cfg.paths.dirPNG;
                std::filesystem::path camDirPGM = camBase / cfg.paths.dirPGM;
                std::filesystem::path camDirPLY = camBase / cfg.paths.dirPLY;

                std::filesystem::create_directories(camDirPNG);
                std::filesystem::create_directories(camDirPGM);
                std::filesystem::create_directories(camDirPLY);

                if (opt == "1")
                {
                    std::string fDisp = camPrefix + "_disparity_" + tag + ".pgm";
                    std::string fRect = camPrefix + "_rectified_" + tag + ".png";

                    auto pDisp = (camDirPGM / fDisp).string();
                    auto pRect = (camDirPNG / fRect).string();

                    bool okDisp = a.drv.SaveDisparityPGM(set, pDisp);
                    bool okRect = a.drv.SaveRectifiedPNG(set, pRect);

                    std::cout << a.cfg->name << " Guardado\n";
                    std::cout << " - " << pDisp << " " << (okDisp ? "OK" : "FAIL") << "\n";
                    std::cout << " - " << pRect << " " << (okRect ? "OK" : "FAIL") << "\n";
                }
                else if (opt == "2")
                {
                    a.drv.ReadScan3DParams(a.s3d);

                    std::string fPly = camPrefix + "_cloud_" + tag + ".ply";
                    auto pPly = (camDirPLY / fPly).string();

                    std::cout << "\n--- " << a.cfg->name << " Generar PLY filtrado ---\n";
                    if (a.drv.SavePointCloudPLY_Filtered(set, a.s3d, a.cfg->params, a.cfg->mount, pPly))
                        std::cout << a.cfg->name << " OK guardado " << pPly << "\n";
                    else
                        std::cout << a.cfg->name << " FAIL PLY\n";
                }
                else if (opt == "3")
                {
                    float zCenter = 0.f;
                    float zBulto = 0.f;
                    int used = 0;

                    bool okC = a.drv.GetDistanceCentralPointM(set, a.s3d, zCenter);
                    bool okB = a.drv.GetDistanceToBultoM_Debug(set, a.s3d, a.cfg->params, a.cfg->mount, zBulto, used);

                    std::cout << a.cfg->name << " Distancias\n";
                    std::cout << " - Centro " << (okC ? std::to_string(zCenter) : std::string("FAIL")) << " m\n";
                    std::cout << " - Cara bulto " << (okB ? std::to_string(zBulto) : std::string("FAIL")) << " m puntos " << used << "\n";
                }

                ReleaseImageList(set);
            };

        for (auto& a : act) DoCam(a);
    }

    for (auto& a : act)
    {
        if (!a.available) continue;
        a.drv.StopAcquisition();
        a.drv.Close();
    }

    cams.Clear();
    system->ReleaseInstance();

    std::cout << "Saliendo\n";
    return 0;
}
