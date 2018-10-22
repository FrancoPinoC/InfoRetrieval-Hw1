#include "utils.h"
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>

// With thanks to https://stackoverflow.com/a/4430900/4192226
std::string extractNameFromPath(const std::string &fullPath) {
    // To be file name with the extension (if it has one)
    std::string filename;
    // Only the name without the extension
    std::string name;
    size_t sep = fullPath.find_last_of("\\/");
    if(sep != std::string::npos) {
        filename = fullPath.substr(sep + 1, fullPath.size() - sep - 1);
    }
    size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) {name = filename.substr(0, dot);}
    else {name = filename;}
    return name;
}

unsigned long amountSampled(unsigned long totalFrames, int sampleRate) {
    return (((totalFrames - 1)/ sampleRate) + 1);
}

// Euclidean distance func (without square-rooting)
long euclideanDist(std::vector<int> &a, std::vector<int> &b) {
    long res = 0;
    // Assumes they're both the same size
    for(unsigned long i = 0; i < a.size(); i++){
        long diff = a[i] - b[i];
        res += diff*diff;
    }
    return res;
}

// Euclidian distance, but one argument is a Mat that we are gonna treat as a vector and the other actually a vector.
long matVectEuclideanDist(cv::Mat &frame, std::vector<int> &desc) {
    // Because I mean, why go over that Mat twice (once to make it a vect, another to caluclate dist)?
    long res = 0;
    unsigned long vecIndex = 0;
    for(int row = 0; row < frame.rows; row++){
        for(int col = 0; col < frame.cols; col++) {
            long diff = (int)frame.at<uchar>(row, col) - desc[vecIndex++];
            res += diff*diff;
        }
    }
    return res;
}

// In case I actually end up just keeping everything in memory all the time
long matMatEcuclideanDist(cv::Mat &a, cv::Mat &b) {
    // Because I mean, why go over that Mat twice (once to make it a vect, another to caluclate dist)?
    long res = 0;
    unsigned long vecIndex = 0;
    for(int row = 0; row < a.rows; row++){
        for(int col = 0; col < a.cols; col++) {
            long diff = (int)a.at<uchar>(row, col) - (int)b.at<uchar>(row, col);
            res += diff*diff;
        }
    }
    return res;
}
