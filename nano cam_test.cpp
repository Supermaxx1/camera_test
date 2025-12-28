#include <opencv2/opencv.hpp>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <cmath>

// --- Config Reader ---
std::map<std::string, std::map<std::string, std::string>> readINI(const std::string& filename) {
    std::map<std::string, std::map<std::string, std::string>> data;
    std::ifstream file(filename);
    std::string line, section;
    while(std::getline(file, line)) {
        if(line.empty() || line[0] == '#') continue;
        if(line[0] == '[') section = line.substr(1, line.find(']')-1);
        else {
            size_t pos = line.find('=');
            if(pos != std::string::npos) data[section][line.substr(0, pos)] = line.substr(pos+1);
        }
    }
    return data;
}

// --- Deskew Logic ---
cv::Mat deskewImage(const cv::Mat& src, const std::map<std::string, std::string>& skew) {
    std::vector<cv::Point2f> srcPts = {
        cv::Point2f(std::stof(skew.at("point1x")), std::stof(skew.at("point1y"))),
        cv::Point2f(std::stof(skew.at("point2x")), std::stof(skew.at("point2y"))),
        cv::Point2f(std::stof(skew.at("point3x")), std::stof(skew.at("point3y"))),
        cv::Point2f(std::stof(skew.at("point4x")), std::stof(skew.at("point4y")))
    };
    std::vector<cv::Point2f> dstPts = {cv::Point2f(0,0), cv::Point2f(799,0), cv::Point2f(799,799), cv::Point2f(0,799)};
    cv::Mat M = cv::getPerspectiveTransform(srcPts, dstPts);
    cv::Mat result;
    cv::warpPerspective(src, result, M, cv::Size(800, 800));
    return result;
}

// --- NEW: Find True Center (Blue Dot) ---
cv::Point2f findTrueCenter(const cv::Mat& templateClean) {
    cv::Mat gray, binary;
    cv::cvtColor(templateClean, gray, cv::COLOR_BGR2GRAY);
    cv::bitwise_not(gray, gray); // Invert (Black becomes White)
    cv::threshold(gray, binary, 200, 255, cv::THRESH_BINARY);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    double maxArea = 0; int maxIdx = -1;
    for(size_t i=0; i<contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if(area > maxArea) { maxArea = area; maxIdx = i; }
    }
    
    if(maxIdx >= 0) {
        cv::Moments m = cv::moments(contours[maxIdx]);
        return cv::Point2f(m.m10/m.m00, m.m01/m.m00);
    }
    return cv::Point2f(400, 400); // Fallback
}

// --- Hole Detection ---
cv::Point2f detectHole(const cv::Mat& templateImg, const cv::Mat& shotImg, cv::Mat& visual, cv::Point2f trueCenter) {
    cv::Mat grayTemplate, grayShot;
    cv::cvtColor(templateImg, grayTemplate, cv::COLOR_BGR2GRAY);
    cv::cvtColor(shotImg, grayShot, cv::COLOR_BGR2GRAY);
    
    cv::GaussianBlur(grayTemplate, grayTemplate, cv::Size(5,5), 0);
    cv::GaussianBlur(grayShot, grayShot, cv::Size(5,5), 0);

    cv::Mat diff;
    cv::absdiff(grayTemplate, grayShot, diff);

    cv::Mat binary;
    cv::threshold(diff, binary, 30, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double bestArea = 0;
    int bestIdx = -1;

    // Filter for bullet size (50-5000 pixels)
    for(size_t i=0; i<contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if(area > 50 && area < 5000) { 
            if(area > bestArea) {
                bestArea = area;
                bestIdx = i;
            }
        }
    }

    if(bestIdx == -1) {
        return cv::Point2f(-1, -1);
    }

    cv::Moments m = cv::moments(contours[bestIdx]);
    cv::Point2f center(m.m10/m.m00, m.m01/m.m00);

    visual = shotImg.clone();
    cv::drawContours(visual, contours, bestIdx, cv::Scalar(0,255,0), 2); // Green Hole
    cv::circle(visual, center, 5, cv::Scalar(0,0,255), -1); // Red Shot
    cv::circle(visual, trueCenter, 5, cv::Scalar(255,0,0), -1); // Blue Center
    
    return center;
}

// --- ISSF Scoring ---
double computeScoreRifle(float distMM) {
    const float rings[] = {0.25, 2.75, 5.25, 7.75, 10.25, 12.75, 15.25, 17.75, 20.25, 22.75};
    const int scores[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    
    for(int i=0; i<10; i++) {
        if(distMM <= rings[i]) {
            double score = 10.9 - (distMM / 2.5);
            if(score > 10.9) score = 10.9;
            if(score < 0) score = 0;
            return score;
        }
    }
    return 0.0;
}

int main() {
    auto config = readINI("image_settings.ini");
    
    float pxPerMM = 10.0f;
    if(config["calibration"].count("px_per_mm")) {
        pxPerMM = std::stof(config["calibration"]["px_per_mm"]);
    }
    
    // Camera setup
    std::string pipeline = "libcamerasrc ! video/x-raw, width=1280, height=960, framerate=30/1 ! videoconvert ! appsink";
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    
    if(!cap.isOpened()) {
        std::cerr << "Error: Camera not accessible!" << std::endl;
        return -1;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "      LIVE SCORING SYSTEM - 10m RIFLE   " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << " [t]  - Capture TEMPLATE (Clean Target) " << std::endl;
    std::cout << " [SPACE] - SCORE SHOT (Detect Hole)     " << std::endl;
    std::cout << " [ESC]- Exit                            " << std::endl;
    
    cv::Mat frame, deskewed, visual, templateClean;
    cv::Point2f trueCenter(400,400);
    bool hasTemplate = false;
    
    while(true) {
        cap >> frame;
        if(frame.empty()) break;
        
        deskewed = deskewImage(frame, config["skew"]);
        cv::imshow("Live View", deskewed);
        
        int key = cv::waitKey(30);
        if(key == 27) break; // ESC
        
        // --- Capture Template ('t') ---
        if(key == 't' || key == 'T') {
            templateClean = deskewed.clone();
            trueCenter = findTrueCenter(templateClean);
            hasTemplate = true;
            std::cout << "Template Captured! Target Center at: " << trueCenter << std::endl;
        }
        
        // --- Score Shot (SPACE) ---
        if(key == ' ') { 
            if(!hasTemplate) {
                std::cout << "Error: Press 't' first to capture clean target!" << std::endl;
            } else {
                cv::Point2f holePos = detectHole(templateClean, deskewed, visual, trueCenter);
                
                if(holePos.x >= 0) {
                    // CALCULATE USING TRUE CENTER (BLUE DOT)
                    float dx = holePos.x - trueCenter.x;
                    float dy = holePos.y - trueCenter.y;
                    float distPx = std::sqrt(dx*dx + dy*dy);
                    float distMM = distPx / pxPerMM;
                    double score = computeScoreRifle(distMM);
                    
                    std::cout << "\n-----------------------------------------" << std::endl;
                    std::cout << " SHOT DETECTED " << std::endl;
                    std::cout << "-----------------------------------------" << std::endl;
                    std::cout << " Position: (" << std::fixed << std::setprecision(1) << holePos.x << ", " << holePos.y << ")" << std::endl;
                    std::cout << " Distance: " << distMM << " mm" << std::endl;
                    std::cout << " SCORE:    " << score << std::endl;
                    std::cout << "-----------------------------------------" << std::endl;
                    
                    cv::imshow("Shot Result", visual);
                    
                    // Update template to include this hole (so next shot is detected as new)
                    templateClean = deskewed.clone();
                } else {
                    std::cout << "Miss (No hole detected)" << std::endl;
                }
            }
        }
    }
    return 0;
}
