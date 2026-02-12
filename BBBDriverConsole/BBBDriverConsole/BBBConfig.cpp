#include "BBBConfig.h"
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <utility>
#include <cctype>
#include <cmath>

static inline std::string Trim(std::string s)
{
    auto isspace2 = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isspace2((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace2((unsigned char)s.back())) s.pop_back();
    return s;
}

static inline std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string DefaultOrientForIndex(int index0Based)
{
    if (index0Based == 0) return "izq";
    if (index0Based == 1) return "der";
    return "cenital";
}

static std::string CanonicalOrient(std::string s)
{
    s = ToLower(Trim(std::move(s)));
    if (s == "izq" || s == "izquierda" || s == "left") return "izq";
    if (s == "der" || s == "derecha" || s == "right") return "der";
    if (s == "cen" || s == "cenital" || s == "top") return "cenital";
    return s;
}

static bool NearlyEqualF(float a, float b, float eps = 1e-6f)
{
    float da = std::fabs(a - b);
    float m = (std::max)(1.0f, (std::max)(std::fabs(a), std::fabs(b)));
    return da <= eps * m;
}

static bool ControlEqual(const BBBControl& a, const BBBControl& b)
{
    return std::fabs(a.exposureUs - b.exposureUs) <= 1e-6 &&
           std::fabs(a.gainDb - b.gainDb) <= 1e-6;
}

static bool ParamsEqual(const BBBParams& a, const BBBParams& b)
{
    return
        NearlyEqualF(a.minRangeM, b.minRangeM) &&
        NearlyEqualF(a.maxRangeM, b.maxRangeM) &&
        a.roiMinXPct == b.roiMinXPct &&
        a.roiMaxXPct == b.roiMaxXPct &&
        a.roiMinYPct == b.roiMinYPct &&
        a.roiMaxYPct == b.roiMaxYPct &&
        a.decimationFactor == b.decimationFactor &&
        a.applySpeckleFilter == b.applySpeckleFilter &&
        a.maxSpeckleSize == b.maxSpeckleSize &&
        a.speckleThreshold == b.speckleThreshold &&
        a.applyMedian3x3 == b.applyMedian3x3 &&
        NearlyEqualF(a.voxelLeafM, b.voxelLeafM) &&
        NearlyEqualF(a.outlierRadiusM, b.outlierRadiusM) &&
        a.outlierMinNeighbors == b.outlierMinNeighbors &&
        a.keepLargestCluster == b.keepLargestCluster &&
        a.enableGroundPlaneFilter == b.enableGroundPlaneFilter &&
        NearlyEqualF(a.groundBandPct, b.groundBandPct) &&
        NearlyEqualF(a.groundRansacThrM, b.groundRansacThrM) &&
        a.groundRansacIters == b.groundRansacIters &&
        NearlyEqualF(a.groundCutMarginM, b.groundCutMarginM) &&
        a.enableFrontDepthClamp == b.enableFrontDepthClamp &&
        NearlyEqualF(a.frontFacePercentile, b.frontFacePercentile) &&
        NearlyEqualF(a.frontDepthBandM, b.frontDepthBandM) &&
        NearlyEqualF(a.faceSlabM, b.faceSlabM) &&
        NearlyEqualF(a.dimPercentileLow, b.dimPercentileLow) &&
        NearlyEqualF(a.dimPercentileHigh, b.dimPercentileHigh) &&
        a.colorMode == b.colorMode &&
        a.plyBinary == b.plyBinary &&
        NearlyEqualF(a.hardMaxZM, b.hardMaxZM) &&
        NearlyEqualF(a.groundMinHeightM, b.groundMinHeightM) &&
        NearlyEqualF(a.bultoFacePercentile, b.bultoFacePercentile);
}

static bool ParseIni(const std::string& path, std::unordered_map<std::string, std::string>& kv)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string section;
    std::string line;

    while (std::getline(f, line))
    {
        auto posc = line.find(';');
        if (posc != std::string::npos) line = line.substr(0, posc);
        posc = line.find('#');
        if (posc != std::string::npos) line = line.substr(0, posc);

        line = Trim(line);
        if (line.empty()) continue;

        if (line.size() >= 3 && line.front() == '[' && line.back() == ']')
        {
            section = Trim(line.substr(1, line.size() - 2));
            section = ToLower(section);
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = ToLower(Trim(line.substr(0, eq)));
        std::string val = Trim(line.substr(eq + 1));

        std::string full = section.empty() ? key : (section + "." + key);
        kv[full] = val;
    }

    return true;
}

static bool HasKey(const std::unordered_map<std::string, std::string>& kv, const std::string& k)
{
    return kv.find(ToLower(k)) != kv.end();
}

static bool GetStr(const std::unordered_map<std::string, std::string>& kv, const std::string& k, std::string& out)
{
    auto it = kv.find(ToLower(k));
    if (it == kv.end()) return false;
    out = it->second;
    return true;
}

static bool GetI(const std::unordered_map<std::string, std::string>& kv, const std::string& k, int& out)
{
    std::string s;
    if (!GetStr(kv, k, s)) return false;
    out = std::stoi(s);
    return true;
}

static bool GetU64(const std::unordered_map<std::string, std::string>& kv, const std::string& k, uint64_t& out)
{
    std::string s;
    if (!GetStr(kv, k, s)) return false;
    long long v = std::stoll(s);
    if (v < 0) v = 0;
    out = (uint64_t)v;
    return true;
}

static bool GetF(const std::unordered_map<std::string, std::string>& kv, const std::string& k, float& out)
{
    std::string s;
    if (!GetStr(kv, k, s)) return false;
    out = std::stof(s);
    return true;
}

static bool GetD(const std::unordered_map<std::string, std::string>& kv, const std::string& k, double& out)
{
    std::string s;
    if (!GetStr(kv, k, s)) return false;
    out = std::stod(s);
    return true;
}

static bool GetB(const std::unordered_map<std::string, std::string>& kv, const std::string& k, bool& out)
{
    std::string s;
    if (!GetStr(kv, k, s)) return false;
    s = ToLower(Trim(s));
    out = (s == "1" || s == "true" || s == "yes" || s == "on");
    return true;
}

static void LoadMount(const std::unordered_map<std::string, std::string>& kv, const std::string& prefix, BBBCameraMount& m)
{
    GetF(kv, prefix + ".alturacamaram", m.alturaCamaraM);
    GetF(kv, prefix + ".disthorizarc0m", m.distHorizArc0M);
    GetF(kv, prefix + ".pitchdeg", m.pitchDeg);
}

static void LoadParams(const std::unordered_map<std::string, std::string>& kv, const std::string& prefix, BBBParams& p)
{
    GetF(kv, prefix + ".minrangem", p.minRangeM);
    GetF(kv, prefix + ".maxrangem", p.maxRangeM);

    GetI(kv, prefix + ".roiminxpct", p.roiMinXPct);
    GetI(kv, prefix + ".roimaxxpct", p.roiMaxXPct);
    GetI(kv, prefix + ".roiminypct", p.roiMinYPct);
    GetI(kv, prefix + ".roimaxypct", p.roiMaxYPct);

    GetI(kv, prefix + ".decimationfactor", p.decimationFactor);

    GetB(kv, prefix + ".applyspecklefilter", p.applySpeckleFilter);
    GetI(kv, prefix + ".maxspecklesize", p.maxSpeckleSize);
    GetI(kv, prefix + ".specklethreshold", p.speckleThreshold);

    GetB(kv, prefix + ".applymedian3x3", p.applyMedian3x3);

    GetF(kv, prefix + ".voxelleafm", p.voxelLeafM);

    GetF(kv, prefix + ".outlierradiusm", p.outlierRadiusM);
    GetI(kv, prefix + ".outlierminneighbors", p.outlierMinNeighbors);

    GetB(kv, prefix + ".keeplargestcluster", p.keepLargestCluster);

    GetB(kv, prefix + ".enablegroundplanefilter", p.enableGroundPlaneFilter);
    GetF(kv, prefix + ".groundbandpct", p.groundBandPct);
    GetF(kv, prefix + ".groundransacthrm", p.groundRansacThrM);
    GetI(kv, prefix + ".groundransaciters", p.groundRansacIters);
    GetF(kv, prefix + ".groundcutmarginm", p.groundCutMarginM);

    GetB(kv, prefix + ".enablefrontdepthclamp", p.enableFrontDepthClamp);
    GetF(kv, prefix + ".frontfacepercentile", p.frontFacePercentile);
    GetF(kv, prefix + ".frontdepthbandm", p.frontDepthBandM);

    GetF(kv, prefix + ".faceslabm", p.faceSlabM);

    GetF(kv, prefix + ".dimpercentilelow", p.dimPercentileLow);
    GetF(kv, prefix + ".dimpercentilehigh", p.dimPercentileHigh);

    GetI(kv, prefix + ".colormode", p.colorMode);
    GetB(kv, prefix + ".plybinary", p.plyBinary);

    GetF(kv, prefix + ".hardmaxzm", p.hardMaxZM);
    GetF(kv, prefix + ".groundminheightm", p.groundMinHeightM);

    GetF(kv, prefix + ".bultofacepercentile", p.bultoFacePercentile);
}

static void LoadControl(const std::unordered_map<std::string, std::string>& kv, const std::string& prefix, BBBControl& c)
{
    GetD(kv, prefix + ".exposureus", c.exposureUs);
    GetD(kv, prefix + ".gaindb", c.gainDb);
}

static void WriteSection(std::ofstream& f, const std::string& name)
{
    f << "[" << name << "]\n";
}

static void WriteKV(std::ofstream& f, const std::string& k, const std::string& v) { f << k << "=" << v << "\n"; }
static void WriteKV(std::ofstream& f, const std::string& k, int v) { f << k << "=" << v << "\n"; }
static void WriteKV(std::ofstream& f, const std::string& k, uint64_t v) { f << k << "=" << v << "\n"; }
static void WriteKV(std::ofstream& f, const std::string& k, float v) { f << k << "=" << v << "\n"; }
static void WriteKV(std::ofstream& f, const std::string& k, double v) { f << k << "=" << v << "\n"; }
static void WriteKV(std::ofstream& f, const std::string& k, bool v) { f << k << "=" << (v ? 1 : 0) << "\n"; }

static void SaveMount(std::ofstream& f, const BBBCameraMount& m)
{
    WriteKV(f, "alturaCamaraM", m.alturaCamaraM);
    WriteKV(f, "distHorizArc0M", m.distHorizArc0M);
    WriteKV(f, "pitchDeg", m.pitchDeg);
}

static void SaveParams(std::ofstream& f, const BBBParams& p)
{
    WriteKV(f, "minRangeM", p.minRangeM);
    WriteKV(f, "maxRangeM", p.maxRangeM);

    WriteKV(f, "roiMinXPct", p.roiMinXPct);
    WriteKV(f, "roiMaxXPct", p.roiMaxXPct);
    WriteKV(f, "roiMinYPct", p.roiMinYPct);
    WriteKV(f, "roiMaxYPct", p.roiMaxYPct);

    WriteKV(f, "decimationFactor", p.decimationFactor);

    WriteKV(f, "applySpeckleFilter", p.applySpeckleFilter);
    WriteKV(f, "maxSpeckleSize", p.maxSpeckleSize);
    WriteKV(f, "speckleThreshold", p.speckleThreshold);

    WriteKV(f, "applyMedian3x3", p.applyMedian3x3);

    WriteKV(f, "voxelLeafM", p.voxelLeafM);

    WriteKV(f, "outlierRadiusM", p.outlierRadiusM);
    WriteKV(f, "outlierMinNeighbors", p.outlierMinNeighbors);

    WriteKV(f, "keepLargestCluster", p.keepLargestCluster);

    WriteKV(f, "enableGroundPlaneFilter", p.enableGroundPlaneFilter);
    WriteKV(f, "groundBandPct", p.groundBandPct);
    WriteKV(f, "groundRansacThrM", p.groundRansacThrM);
    WriteKV(f, "groundRansacIters", p.groundRansacIters);
    WriteKV(f, "groundCutMarginM", p.groundCutMarginM);

    WriteKV(f, "enableFrontDepthClamp", p.enableFrontDepthClamp);
    WriteKV(f, "frontFacePercentile", p.frontFacePercentile);
    WriteKV(f, "frontDepthBandM", p.frontDepthBandM);

    WriteKV(f, "faceSlabM", p.faceSlabM);

    WriteKV(f, "dimPercentileLow", p.dimPercentileLow);
    WriteKV(f, "dimPercentileHigh", p.dimPercentileHigh);

    WriteKV(f, "colorMode", p.colorMode);
    WriteKV(f, "plyBinary", p.plyBinary);

    WriteKV(f, "hardMaxZM", p.hardMaxZM);
    WriteKV(f, "groundMinHeightM", p.groundMinHeightM);

    WriteKV(f, "bultoFacePercentile", p.bultoFacePercentile);
}

static void SaveControl(std::ofstream& f, const BBBControl& c)
{
    WriteKV(f, "exposureUs", c.exposureUs);
    WriteKV(f, "gainDb", c.gainDb);
}

std::string BBBConfig::MakeAutoName(const BBBAppConfig& cfg, const std::string& serial, int index1Based)
{
    if (!serial.empty())
        return cfg.namePrefix + serial;
    return cfg.namePrefix + std::string("UNASSIGNED") + std::to_string(index1Based);
}

bool BBBConfig::LoadIni(const std::string& iniPath, BBBAppConfig& out)
{
    std::unordered_map<std::string, std::string> kv;
    if (!ParseIni(iniPath, kv)) return false;

    GetStr(kv, "general.outputdir", out.paths.outputDir);
    GetStr(kv, "general.dirpng", out.paths.dirPNG);
    GetStr(kv, "general.dirpgm", out.paths.dirPGM);
    GetStr(kv, "general.dirply", out.paths.dirPLY);
    GetU64(kv, "general.capturetimeoutms", out.paths.captureTimeoutMs);

    GetI(kv, "general.maxcameras", out.maxCameras);
    GetB(kv, "general.autoadddetectedcameras", out.autoAddDetectedCameras);
    GetB(kv, "general.autonamefromserial", out.autoNameFromSerial);
    GetStr(kv, "general.nameprefix", out.namePrefix);

    if (out.maxCameras < 1) out.maxCameras = 1;
    if (out.maxCameras > 3) out.maxCameras = 3;

    LoadMount(kv, "defaults", out.defaultMount);
    LoadParams(kv, "defaults.params", out.defaultParams);
    LoadControl(kv, "defaults.control", out.defaultControl);

    out.cameras.clear();
    out.cameras.reserve((size_t)out.maxCameras);

    for (int i = 0; i < out.maxCameras; ++i)
    {
        std::string base = "camera." + std::to_string(i);

        bool hasAny =
            HasKey(kv, base + ".serial") ||
            HasKey(kv, base + ".name") ||
            HasKey(kv, base + ".enabled") ||
            HasKey(kv, base + ".orient") ||
            HasKey(kv, base + ".side");

        CameraConfig c;
        c.orient = DefaultOrientForIndex(i);
        c.mount = out.defaultMount;
        c.params = out.defaultParams;
        c.control = out.defaultControl;

        if (hasAny)
        {
            GetB(kv, base + ".enabled", c.enabled);
            GetStr(kv, base + ".serial", c.serial);
            GetStr(kv, base + ".name", c.name);

            // ARR preferimos orient y aceptamos side por compatibilidad
            if (!GetStr(kv, base + ".orient", c.orient))
                GetStr(kv, base + ".side", c.orient);

            c.orient = CanonicalOrient(c.orient);

            LoadMount(kv, base, c.mount);
            LoadParams(kv, base + ".params", c.params);
            LoadControl(kv, base + ".control", c.control);
        }
        else
        {
            c.enabled = true;
        }

        if (c.orient.empty())
            c.orient = DefaultOrientForIndex(i);

        if (c.name.empty() && out.autoNameFromSerial)
            c.name = MakeAutoName(out, c.serial, i + 1);

        if (c.name.empty())
            c.name = MakeAutoName(out, c.serial, i + 1);

        out.cameras.push_back(c);
    }

    return true;
}

bool BBBConfig::SaveIni(const std::string& iniPath, const BBBAppConfig& cfgIn)
{
    BBBAppConfig cfg = cfgIn;

    if (cfg.maxCameras < 1) cfg.maxCameras = 1;
    if (cfg.maxCameras > 3) cfg.maxCameras = 3;

    if ((int)cfg.cameras.size() < cfg.maxCameras)
    {
        int need = cfg.maxCameras - (int)cfg.cameras.size();
        for (int i = 0; i < need; ++i)
        {
            CameraConfig c;
            c.enabled = true;
            c.mount = cfg.defaultMount;
            c.params = cfg.defaultParams;
            c.control = cfg.defaultControl;
            c.orient = DefaultOrientForIndex((int)cfg.cameras.size());
            c.name = MakeAutoName(cfg, "", (int)cfg.cameras.size() + 1);
            cfg.cameras.push_back(c);
        }
    }
    if ((int)cfg.cameras.size() > cfg.maxCameras)
        cfg.cameras.resize(cfg.maxCameras);

    std::ofstream f(iniPath, std::ios::binary);
    if (!f.is_open()) return false;

    WriteSection(f, "General");
    WriteKV(f, "outputDir", cfg.paths.outputDir);
    WriteKV(f, "dirPNG", cfg.paths.dirPNG);
    WriteKV(f, "dirPGM", cfg.paths.dirPGM);
    WriteKV(f, "dirPLY", cfg.paths.dirPLY);
    WriteKV(f, "captureTimeoutMs", cfg.paths.captureTimeoutMs);
    WriteKV(f, "maxCameras", cfg.maxCameras);
    WriteKV(f, "autoAddDetectedCameras", cfg.autoAddDetectedCameras);
    WriteKV(f, "autoNameFromSerial", cfg.autoNameFromSerial);
    WriteKV(f, "namePrefix", cfg.namePrefix);
    f << "\n";

    WriteSection(f, "Defaults");
    SaveMount(f, cfg.defaultMount);
    f << "\n";

    WriteSection(f, "Defaults.Params");
    SaveParams(f, cfg.defaultParams);
    f << "\n";

    WriteSection(f, "Defaults.Control");
    SaveControl(f, cfg.defaultControl);
    f << "\n";

    for (int i = 0; i < cfg.maxCameras; ++i)
    {
        CameraConfig c = cfg.cameras[i];

        if (c.orient.empty()) c.orient = DefaultOrientForIndex(i);
        c.orient = CanonicalOrient(c.orient);

        if (c.name.empty() && cfg.autoNameFromSerial)
            c.name = MakeAutoName(cfg, c.serial, i + 1);

        WriteSection(f, "Camera." + std::to_string(i));
        WriteKV(f, "enabled", c.enabled);
        WriteKV(f, "serial", c.serial);
        WriteKV(f, "name", c.name);
        WriteKV(f, "orient", c.orient);
        SaveMount(f, c.mount);
        f << "\n";

        if (!ParamsEqual(c.params, cfg.defaultParams))
        {
            WriteSection(f, "Camera." + std::to_string(i) + ".Params");
            SaveParams(f, c.params);
            f << "\n";
        }

        if (!ControlEqual(c.control, cfg.defaultControl))
        {
            WriteSection(f, "Camera." + std::to_string(i) + ".Control");
            SaveControl(f, c.control);
            f << "\n";
        }
    }

    return true;
}

bool BBBConfig::EnsureDetectedCameras(
    BBBAppConfig& cfg,
    const std::vector<std::string>& detectedStereoSerials,
    bool& outChanged)
{
    outChanged = false;
    if (!cfg.autoAddDetectedCameras) return true;

    if (cfg.maxCameras < 1) cfg.maxCameras = 1;
    if (cfg.maxCameras > 3) cfg.maxCameras = 3;

    // ARR limpiamos duplicados dentro del propio cfg
    for (int i = 0; i < (int)cfg.cameras.size(); ++i)
    {
        if (cfg.cameras[i].serial.empty()) continue;

        for (int j = i + 1; j < (int)cfg.cameras.size(); ++j)
        {
            if (cfg.cameras[j].serial == cfg.cameras[i].serial && !cfg.cameras[j].serial.empty())
            {
                cfg.cameras[j].serial.clear();
                cfg.cameras[j].name = MakeAutoName(cfg, "", j + 1);
                if (cfg.cameras[j].orient.empty()) cfg.cameras[j].orient = DefaultOrientForIndex(j);
                cfg.cameras[j].enabled = true;
                outChanged = true;
            }
        }
    }

    // ARR hacemos lista de seriales detectados unica
    std::vector<std::string> uniqueDetected;
    uniqueDetected.reserve(detectedStereoSerials.size());
    for (const auto& s : detectedStereoSerials)
    {
        if (s.empty()) continue;
        if (std::find(uniqueDetected.begin(), uniqueDetected.end(), s) != uniqueDetected.end())
            continue;
        uniqueDetected.push_back(s);
    }

    auto HasSerial = [&](const std::string& s)
        {
            for (const auto& c : cfg.cameras)
                if (!c.serial.empty() && c.serial == s)
                    return true;
            return false;
        };

    // ARR primero rellenamos huecos serial vacios en Camera.0..2
    for (const auto& s : uniqueDetected)
    {
        if (HasSerial(s)) continue;

        bool placed = false;
        for (int i = 0; i < (int)cfg.cameras.size() && i < cfg.maxCameras; ++i)
        {
            if (cfg.cameras[i].serial.empty())
            {
                cfg.cameras[i].serial = s;

                if (cfg.cameras[i].orient.empty())
                    cfg.cameras[i].orient = DefaultOrientForIndex(i);

                cfg.cameras[i].orient = CanonicalOrient(cfg.cameras[i].orient);

                if (cfg.autoNameFromSerial)
                    cfg.cameras[i].name = MakeAutoName(cfg, s, i + 1);

                outChanged = true;
                placed = true;
                break;
            }
        }

        if (placed) continue;

        if ((int)cfg.cameras.size() >= cfg.maxCameras)
            continue;

        CameraConfig c;
        c.enabled = true;
        c.serial = s;
        c.orient = DefaultOrientForIndex((int)cfg.cameras.size());
        c.mount = cfg.defaultMount;
        c.params = cfg.defaultParams;
        c.control = cfg.defaultControl;
        c.name = cfg.autoNameFromSerial ? MakeAutoName(cfg, s, (int)cfg.cameras.size() + 1) : MakeAutoName(cfg, "", (int)cfg.cameras.size() + 1);

        cfg.cameras.push_back(c);
        outChanged = true;
    }

    // ARR garantizamos que hay exactamente maxCameras entradas
    while ((int)cfg.cameras.size() < cfg.maxCameras)
    {
        CameraConfig c;
        c.enabled = true;
        c.orient = DefaultOrientForIndex((int)cfg.cameras.size());
        c.mount = cfg.defaultMount;
        c.params = cfg.defaultParams;
        c.control = cfg.defaultControl;
        c.name = MakeAutoName(cfg, "", (int)cfg.cameras.size() + 1);
        cfg.cameras.push_back(c);
        outChanged = true;
    }
    if ((int)cfg.cameras.size() > cfg.maxCameras)
    {
        cfg.cameras.resize(cfg.maxCameras);
        outChanged = true;
    }

    return true;
}
