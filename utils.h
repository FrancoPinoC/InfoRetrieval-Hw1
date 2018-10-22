// Small utility functions go here.
#ifndef RETRIEVALT1_UTILS_H
#define RETRIEVALT1_UTILS_H
#include <iostream>
#include <opencv2/opencv.hpp>

std::string extractNameFromPath(const std::string &fullPath);

long euclideanDist(std::vector<int> &a, std::vector<int> &b);
long matVectEuclideanDist(cv::Mat &frame, std::vector<int> &desc);
long matMatEcuclideanDist(cv::Mat &a, cv::Mat &b);
unsigned long amountSampled(unsigned long totalFrames, int sampleRate);
#endif //RETRIEVALT1_UTILS_H
