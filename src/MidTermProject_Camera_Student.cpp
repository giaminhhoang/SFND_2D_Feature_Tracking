/* INCLUDES FOR THIS PROJECT */
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <limits>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>

#include "dataStructures.h"
#include "matching2D.hpp"

using namespace std;

/* MAIN PROGRAM */
int main(int argc, const char *argv[])
{

    /* INIT VARIABLES AND DATA STRUCTURES */

    // data location
    string dataPath = "../";

    // camera
    string imgBasePath = dataPath + "images/";
    string imgPrefix = "KITTI/2011_09_26/image_00/data/000000"; // left camera, color
    string imgFileType = ".png";
    int imgStartIndex = 0; // first file index to load (assumes Lidar and camera names have identical naming convention)
    int imgEndIndex = 9;   // last file index to load
    int imgFillWidth = 4;  // no. of digits which make up the file index (e.g. img-0001.png)

    // misc
    int dataBufferSize = 2;       // no. of images which are held in memory (ring buffer) at the same time
    vector<DataFrame> dataBuffer; // list of data frames which are held in memory at the same time
    bool bVis = false;            // visualize results
    
    // prepare the  result synthesis
    std::ofstream results;
    results.open("results.csv");
    results << "Detector, Descriptor, Image id, No keypoints, No keypoints on preceding vehicle, No matched keypoints, Keypoint detection time (ms), Descriptor extraction time (ms), Matching time (ms)\n"; 

    vector<string> listDetectors = {"SHITOMASI", "HARRIS", "FAST", "BRISK", "ORB", "AKAZE", "SIFT"};
    vector<string> listDescriptors = {"BRISK", "BRIEF", "ORB", "FREAK", "AKAZE", "SIFT"};

    //listDetectors = {"FAST"};
    //listDescriptors = {"BRISK"};

    for (auto det=listDetectors.begin(); det!=listDetectors.end(); det++)
    {
        for (auto des=listDescriptors.begin(); des!=listDescriptors.end(); des++)
        {
            // clear the buffer
            dataBuffer.clear();
            
            // Detector, descriptor, and matching configuration
            string detectorType = *det;
            string descriptorType = *des;
            string matcherType = "MAT_BF";        // MAT_BF, MAT_FLANN
            string descriptorFamily = descriptorType.compare("SIFT") == 0 ? "DES_HOG" : "DES_BINARY";
            string selectorType = "SEL_KNN";       // SEL_NN, SEL_KNN

            if ((descriptorType.compare("AKAZE") == 0 && detectorType.compare("AKAZE") != 0) ||
                (descriptorType.compare("ORB") == 0 && detectorType.compare("SIFT") == 0))
                continue;

            /* MAIN LOOP OVER ALL IMAGES */

            for (size_t imgIndex = 0; imgIndex <= imgEndIndex - imgStartIndex; imgIndex++)
            {
                /* LOAD IMAGE INTO BUFFER */

                // assemble filenames for current index
                ostringstream imgNumber;
                imgNumber << setfill('0') << setw(imgFillWidth) << imgStartIndex + imgIndex;
                string imgFullFilename = imgBasePath + imgPrefix + imgNumber.str() + imgFileType;

                // load image from file and convert to grayscale
                cv::Mat img, imgGray;
                img = cv::imread(imgFullFilename);
                cv::cvtColor(img, imgGray, cv::COLOR_BGR2GRAY);

                //// STUDENT ASSIGNMENT
                //// TASK MP.1 -> replace the following code with ring buffer of size dataBufferSize

                // push image into data frame buffer
                DataFrame frame;
                frame.cameraImg = imgGray;
                if (dataBuffer.size() == dataBufferSize)
                    dataBuffer.erase(dataBuffer.begin());

                dataBuffer.push_back(frame);

                //// EOF STUDENT ASSIGNMENT
                cout << "#1 : LOAD IMAGE INTO BUFFER done" << endl;

                /* DETECT IMAGE KEYPOINTS */

                // extract 2D keypoints from current image
                vector<cv::KeyPoint> keypoints; // create empty feature list for current image
                // string detectorType = "SIFT";

                //// STUDENT ASSIGNMENT
                //// TASK MP.2 -> add the following keypoint detectors in file matching2D.cpp and enable string-based selection based on detectorType
                //// -> HARRIS, FAST, BRISK, ORB, AKAZE, SIFT
                double detectionTime;
                if (detectorType.compare("SHITOMASI") == 0)
                {
                    detKeypointsShiTomasi(keypoints, imgGray, detectionTime, false);
                }
                else if (detectorType.compare("HARRIS") == 0)
                {
                    detKeypointsHarris(keypoints, imgGray, detectionTime, false);    
                } 
                else 
                {
                    detKeypointsModern(keypoints, imgGray, detectorType, detectionTime,false);
                } 
                int numberOfKeypoints = keypoints.size();
                //// EOF STUDENT ASSIGNMENT

                //// STUDENT ASSIGNMENT
                //// TASK MP.3 -> only keep keypoints on the preceding vehicle

                // only keep keypoints on the preceding vehicle
                bool bFocusOnVehicle = true;
                cv::Rect vehicleRect(535, 180, 180, 150);
                if (bFocusOnVehicle)
                {   
                    
                    for (auto it = keypoints.begin(); it!= keypoints.end(); it++)
                    {   
                        float p_x = (*it).pt.x; 
                        float p_y = (*it).pt.y; 
                        if ((p_x < vehicleRect.x) || (p_x > vehicleRect.x + vehicleRect.width) ||
                            (p_y < vehicleRect.y) || (p_y > vehicleRect.y + vehicleRect.height))
                        {
                            keypoints.erase(it);
                            it--;       // decreasing iterator afer earsing
                        } 
                    }
                    
                }
                int numberOfKeypointsInPrecedingVehicle = keypoints.size();
                //// EOF STUDENT ASSIGNMENT

                // optional : limit number of keypoints (helpful for debugging and learning)
                bool bLimitKpts = false;
                if (bLimitKpts)
                {
                    int maxKeypoints = 50;

                    if (detectorType.compare("SHITOMASI") == 0)
                    { // there is no response info, so keep the first 50 as they are sorted in descending quality order
                        keypoints.erase(keypoints.begin() + maxKeypoints, keypoints.end());
                    }
                    cv::KeyPointsFilter::retainBest(keypoints, maxKeypoints);
                    cout << " NOTE: Keypoints have been limited!" << endl;
                }

                // push keypoints and descriptor for current frame to end of data buffer
                (dataBuffer.end() - 1)->keypoints = keypoints;
                cout << "#2 : DETECT KEYPOINTS done" << endl;

                /* EXTRACT KEYPOINT DESCRIPTORS */

                //// STUDENT ASSIGNMENT
                //// TASK MP.4 -> add the following descriptors in file matching2D.cpp and enable string-based selection based on descriptorType
                //// -> BRIEF, ORB, FREAK, AKAZE, SIFT

                cv::Mat descriptors;
                double extractionTime = 0.0;
                // string descriptorType = "SIFT"; // BRIEF, ORB, FREAK, AKAZE, SIFT
                descKeypoints((dataBuffer.end() - 1)->keypoints, (dataBuffer.end() - 1)->cameraImg, descriptors, descriptorType, extractionTime);
                //// EOF STUDENT ASSIGNMENT

                // push descriptors for current frame to end of data buffer
                (dataBuffer.end() - 1)->descriptors = descriptors;

                cout << "#3 : EXTRACT DESCRIPTORS done" << endl;

                int numberOfMatchedKeypoints = 0;
                double matchingTime = 0.0;
                if (dataBuffer.size() > 1) // wait until at least two images have been processed
                {

                    /* MATCH KEYPOINT DESCRIPTORS */

                    vector<cv::DMatch> matches;
                    // string matcherType = "MAT_BF";        // MAT_BF, MAT_FLANN
                    // string descriptorFamily = descriptorType.compare("SIFT") == 0 ? "DES_HOG" : "DES_BINARY";
                    // string selectorType = "SEL_KNN";       // SEL_NN, SEL_KNN

                    //// STUDENT ASSIGNMENT
                    //// TASK MP.5 -> add FLANN matching in file matching2D.cpp
                    //// TASK MP.6 -> add KNN match selection and perform descriptor distance ratio filtering with t=0.8 in file matching2D.cpp
                    
                    matchDescriptors((dataBuffer.end() - 2)->keypoints, (dataBuffer.end() - 1)->keypoints,
                                    (dataBuffer.end() - 2)->descriptors, (dataBuffer.end() - 1)->descriptors,
                                    matches, descriptorFamily, matcherType, selectorType, matchingTime);
                    
                    numberOfMatchedKeypoints = matches.size();
                    //// EOF STUDENT ASSIGNMENT

                    // store matches in current data frame
                    (dataBuffer.end() - 1)->kptMatches = matches;

                    cout << "#4 : MATCH KEYPOINT DESCRIPTORS done" << endl;

                    // visualize matches between current and previous image
                    bVis = false;
                    if (bVis)
                    {
                        cv::Mat matchImg = ((dataBuffer.end() - 1)->cameraImg).clone();
                        cv::drawMatches((dataBuffer.end() - 2)->cameraImg, (dataBuffer.end() - 2)->keypoints,
                                        (dataBuffer.end() - 1)->cameraImg, (dataBuffer.end() - 1)->keypoints,
                                        matches, matchImg,
                                        cv::Scalar::all(-1), cv::Scalar::all(-1),
                                        vector<char>(), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

                        string windowName = "Matching keypoints between two camera images";
                        cv::namedWindow(windowName, 7);
                        cv::imshow(windowName, matchImg);
                        cout << "Press key to continue to next image" << endl;
                        cv::waitKey(0); // wait for key to be pressed
                    }
                    bVis = false;
                }

                // write results 
                results << detectorType << "," << descriptorType << "," << imgIndex << "," 
                    << numberOfKeypoints << "," << numberOfKeypointsInPrecedingVehicle << "," << numberOfMatchedKeypoints << "," 
                    << detectionTime << "," << extractionTime << "," << matchingTime <<"\n";

            } // eof loop over all images
        }   // eof loop all descriptors
    } // eof loop all detectors

    results.close();

    return 0;
}
