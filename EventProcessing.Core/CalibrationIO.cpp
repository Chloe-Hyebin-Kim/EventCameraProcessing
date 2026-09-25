#include "pch.h"
#include "CalibrationIO.h"

namespace eventcore
{
    namespace
    {
        double ReadDouble(const cv::FileNode& parent, const char* key, double fallback = 0.0)
        {
            const cv::FileNode n = parent[key];
            if (n.empty() || (!n.isReal() && !n.isInt()))
            {
                return fallback;
            }
            return static_cast<double>(n);
        }

        int ReadInt(const cv::FileNode& parent, const char* key, int fallback = 0)
        {
            const cv::FileNode n = parent[key];
            if (n.empty() || (!n.isInt() && !n.isReal()))
            {
                return fallback;
            }
            return static_cast<int>(static_cast<double>(n));
        }
    }

    bool CalibrationIO::Save(const std::string& nativePath, const CalibrationResult& result, std::string* error)
    {
        try
        {
            cv::FileStorage fs(nativePath, cv::FileStorage::WRITE);
            if (!fs.isOpened())
            {
                if (error) *error = "Could not open file for writing: " + nativePath;
                return false;
            }

            fs << "image_width" << result.imageWidth;
            fs << "image_height" << result.imageHeight;

            fs << "camera_matrix" << "{";
            fs << "fx" << result.fx();
            fs << "fy" << result.fy();
            fs << "cx" << result.cx();
            fs << "cy" << result.cy();
            fs << "}";

            fs << "distortion_coefficients" << "{";
            fs << "k1" << result.dist(0);
            fs << "k2" << result.dist(1);
            fs << "p1" << result.dist(2);
            fs << "p2" << result.dist(3);
            fs << "k3" << result.dist(4);
            fs << "}";

            fs << "rms_reprojection_error" << result.rmsReprojectionError;

            fs << "checkerboard" << "{";
            fs << "rows" << result.checkerboard.innerCornerRows;
            fs << "columns" << result.checkerboard.innerCornerCols;
            fs << "square_size" << result.checkerboard.squareSizeMm;
            fs << "}";

            fs << "number_of_observations" << result.numObservations;
            fs << "model" << (result.model == CalibrationModel::Pinhole ? std::string("pinhole") : std::string("unknown"));

            fs.release();
            return true;
        }
        catch (const cv::Exception& ex)
        {
            if (error) *error = ex.what();
            return false;
        }
    }

    bool CalibrationIO::Load(const std::string& nativePath, CalibrationResult& result, std::string* error)
    {
        try
        {
            cv::FileStorage fs(nativePath, cv::FileStorage::READ);
            if (!fs.isOpened())
            {
                if (error) *error = "Could not open file for reading: " + nativePath;
                return false;
            }

            CalibrationResult r;
            r.imageWidth = ReadInt(fs.root(), "image_width");
            r.imageHeight = ReadInt(fs.root(), "image_height");

            const cv::FileNode km = fs["camera_matrix"];
            const double fx = ReadDouble(km, "fx");
            const double fy = ReadDouble(km, "fy");
            const double cx = ReadDouble(km, "cx");
            const double cy = ReadDouble(km, "cy");

            const cv::FileNode dn = fs["distortion_coefficients"];
            const double k1 = ReadDouble(dn, "k1");
            const double k2 = ReadDouble(dn, "k2");
            const double p1 = ReadDouble(dn, "p1");
            const double p2 = ReadDouble(dn, "p2");
            const double k3 = ReadDouble(dn, "k3");

            r.cameraMatrix = (cv::Mat_<double>(3, 3) << fx, 0.0, cx, 0.0, fy, cy, 0.0, 0.0, 1.0);
            r.distCoeffs = (cv::Mat_<double>(5, 1) << k1, k2, p1, p2, k3);

            r.rmsReprojectionError = ReadDouble(fs.root(), "rms_reprojection_error");

            const cv::FileNode cb = fs["checkerboard"];
            r.checkerboard.innerCornerRows = ReadInt(cb, "rows", r.checkerboard.innerCornerRows);
            r.checkerboard.innerCornerCols = ReadInt(cb, "columns", r.checkerboard.innerCornerCols);
            r.checkerboard.squareSizeMm = ReadDouble(cb, "square_size", r.checkerboard.squareSizeMm);

            r.numObservations = ReadInt(fs.root(), "number_of_observations");
            r.model = CalibrationModel::Pinhole;
            r.success = true;
            r.message = "Loaded from " + nativePath;

            fs.release();
            result = r;
            return true;
        }
        catch (const cv::Exception& ex)
        {
            if (error) *error = ex.what();
            return false;
        }
    }
}
