#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include "utils.h"
using namespace cv;

// Every how many frames are we going to get a frame to make a descriptor?
const int sampleRate = 10;
// Width and height to use when resizing the image to make a descriptor for it
const int resizeW = 20, resizeH = 20;

// Takes all video files in the given input folder, with the given extension, and writes files with descriptors
// for each of them in the given output folder.
int makeVideoDescriptorFiles (const String &inputFolder, const String &extension, const String &outputFolder) {
    std::vector<String> videoPaths;
    String pattern = inputFolder + "/*." + extension;
    // Assumes all relevant files are at the first level of the given directory, so set recursion to false.
    glob(pattern, videoPaths, false);
    for(const String &videoPath: videoPaths) {
        String videoName = extractNameFromPath(videoPath);
        VideoCapture cap(videoPath);
        double totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
        // Amount of frames we are going to use from the video
        auto sampledLength = (unsigned long) ((totalFrames / sampleRate) + 1);
        std::cout << "Processing " << videoPath << ". Total frames: " << totalFrames <<
                     ". Sampled frames: " << sampledLength << std::endl;
        // Vector where we'll store all converted frames
        std::vector<Mat> convertedFrames(sampledLength);
        unsigned long cfIndex = 0;
        // Sampling frames from the video and converting them into a more descriptor(ish) form:
        for (unsigned long i = 0; i < totalFrames; i++) {
            Mat original, converted;
            cap.grab();
            if (i % 10 != 0) { continue; }
            // Retrieve only every 10 frames (starting with the first)
            cap.retrieve(original);
            cvtColor(original, converted, COLOR_BGR2GRAY);
            resize(converted, converted, Size(resizeW, resizeH), 0, 0, INTER_CUBIC);
            // Is 'converted' doing pointer stuff that's gonna mean all convertedFrames are the same in the end ;-;?
            convertedFrames[cfIndex++] = converted;
            // cfIndex to actual index: cfIndex*sampleRate + 1
        }
        // convertedFrames now holds all of every 10 frames from the video, in grayscale in resized to 10,10
        std::cout << "\tSampled and converted frames. Now saving..." << std::endl;
        String outFilePath = outputFolder + "/" + videoName  + ".txt";
        std::ofstream outFile(outFilePath);
        // First line is "[number of used frames] [number of frames in the original video]"
        outFile << sampledLength << ' ' << totalFrames << '\n';
        for (unsigned long i = 0; i < sampledLength; i++) {
            Mat current = convertedFrames[i];
            // Each row is "[frame index (relative to sampled frames)] [space separated pixel values of the frame]"
            outFile << i;
            for(int row = 0; row < current.rows; row++){
                for(int col = 0; col < current.cols; col++) {
                    outFile << ' ' << (int)current.at<uchar>(row, col);
                }
            }
            outFile << '\n';
        }
        outFile.close();
        cap.release();
        if(!outFile) {
            std::cerr << "ERROR: Couldn't save descriptors for " << videoName << std::endl;
            return -1;
        }
        std::cout << "\tDescriptors saved in:\n\t\t" << outFilePath  << "\n\n";
    }
    std::cout << "All videos have been processed \\o/" << std::endl;

}

int main() {
    makeVideoDescriptorFiles("../ads", "mpg", "../ad-descriptors");
//    std::vector<String> videoPaths;
//    String pattern = "../ads/*.mpg";
//    // Assumes all relevant files are at the first level of the given directory, so set recursion to false.
//    glob(pattern, videoPaths, false);
//    VideoCapture cap("../ads/ballerina.mpg");
//    std::cout << "Frames! " << cap.get(cv::CAP_PROP_FRAME_COUNT) << std::endl;
////    cap.set(CV_CAP_PROP_FRAME_WIDTH, 640);
////    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 360);
//    Mat image, gimage;
////    cap.read(image);
//    std::cout << "grabbing " << cap.grab() << std::endl; cap.retrieve(image);
//    cvtColor(image, gimage, COLOR_BGR2GRAY);
//    resize(gimage, gimage, Size(10, 10), 0, 0, INTER_CUBIC);
//    namedWindow("firstframe", 0);
//    imshow("firstframe", gimage);
//    std::cout << "frame(1,1) ";
//    std::cout << (int)gimage.at<uchar>(8,3) << std::endl;
//    waitKey(0);
//    cap.release();
//    destroyAllWindows();

}
