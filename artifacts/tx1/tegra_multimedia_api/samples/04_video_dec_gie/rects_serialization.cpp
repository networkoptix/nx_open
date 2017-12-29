#include "rects_serialization.h"

#include <fstream>
#include <vector>
#include <algorithm>

#include <nx/kit/debug.h>

#include "tegra_video_ini.h"

static bool fileExists(const std::string& filename)
{
    return static_cast<bool>(std::ifstream(filename));
}

bool writeRectsToFile(
    const std::string& filename, const TegraVideo::Rect rects[], int rectCount)
{
    if (fileExists(filename))
    {
        NX_OUTPUT << __func__ << "(): Skipping; file already exists: " << filename;
        return false;
    }

    NX_OUTPUT << __func__ << "(): Writing " << rectCount << " rects to file: " << filename;

    std::ofstream f(filename);
    if (!f.is_open())
    {
        NX_PRINT << "ERROR: Unable to open Rects file for writing: " << filename;
        return true; //< Even in case of error.
    }

    for (int i = 0; i < rectCount; ++i)
    {
        f << "x " << rects[i].x << ", y " << rects[i].y
          << ", w " << rects[i].w << ", h " << rects[i].h << std::endl;
    }

    if (f.bad() || f.fail())
        NX_PRINT << "ERROR: Unable to write to Rects file: " << filename;
    return true; //< Even in case of error.
}

static bool scanFloat(
    float* outValue, std::istream& s, const std::string& expectedLabel, const std::string& line)
{
    std::string label;
    s >> label;
    if (label != expectedLabel)
    {
        NX_PRINT << "ERROR: Invalid line in rects file - missing label \""
            << expectedLabel << "\". Line: \"" << line << "\".";
        return false;
    }

    s >> *outValue;

    // Skip trailing ", ", if any.
    std::string trailing;
    s >> trailing;
    if (!trailing.empty() && trailing != ",")
    {
        NX_PRINT << "ERROR: Invalid line in rects file - unexpected trailing after item \""
            << expectedLabel << "\". Line: \"" << line << "\".";
        return false;
    }

    return true;
}

bool readRectsFromFile(
    const std::string& filename,
    TegraVideo::Rect outRects[],
    int maxRectCount,
    int* const outRectCount)
{
    *outRectCount = 0;
    std::ifstream f(filename);
    if (!f.is_open())
    {
        NX_PRINT << "ERROR: Unable to open Rects file: " << filename;
        return false;
    }

    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty())
            continue;

        ++(*outRectCount);
        if (*outRectCount > maxRectCount)
        {
            NX_PRINT << "ERROR: Too many rects in Rects file: more than " << maxRectCount << ".";
            return false;
        }
        std::istringstream s(line);
        auto& rect = outRects[*outRectCount - 1];
        if (!scanFloat(&rect.x, s, "x", line))
            return false;
        if (!scanFloat(&rect.y, s, "y", line))
            return false;
        if (!scanFloat(&rect.w, s, "w", line))
            return false;
        if (!scanFloat(&rect.h, s, "h", line))
            return false;
    }

    // NOTE: f.fail() is true because std::getline() returned false.
    if (!f.eof())
    {
        NX_PRINT << "ERROR: Unexpected trailing in Rects file: " << filename;
        return false;
    }

    if (f.bad())
    {
        NX_PRINT << "ERROR: I/O error reading Rects file: " << filename;
        return false;
    }

    return true;
}

bool writeModulusToFile(const std::string& filename, int64_t modulus)
{
    if (fileExists(filename))
        NX_PRINT << "WARNING: Modulus file already exists; rewriting.";

    NX_OUTPUT << __func__ << "(): Writing " << modulus << " to file: " << filename;

    std::ofstream f(filename);
    if (!f.is_open())
    {
        NX_PRINT << "ERROR: Unable to open Modulus file for writing: " << filename;
        return false;
    }

    f << modulus;
    f << std::endl;

    if (f.bad() || f.fail())
    {
        NX_PRINT << "ERROR: Unable to write to Modulus file: " << filename;
        return false;
    }

    return true;
}

bool readModulusFromFile(const std::string& filename, int64_t *outModulus)
{
    *outModulus = -1;

    std::ifstream f(filename);
    if (!f.is_open())
    {
        NX_PRINT << "ERROR: Unable to open Modulus file: " << filename;
        return false;
    }

    f >> *outModulus;

    if (f.fail())
    {
        NX_PRINT << "ERROR: Unable to read 64-bit integer from Modulus file: " << filename;
        return false;
    }

    std::string newline;
    f >> newline;
    if (!newline.empty() || !f.eof())
    {
        NX_PRINT << "ERROR: Unexpected trailing in Modulus file: " << filename;
        return false;
    }

    if (f.bad())
    {
        NX_PRINT << "ERROR: I/O error reading Modulus file: " << filename;
        return false;
    }

    return true;
}
