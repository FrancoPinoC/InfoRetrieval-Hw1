#include <opencv2/opencv.hpp>
#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <tuple>

using namespace cv;

// Every how many frames are we going to get a frame to make a descriptor?
const int sampleRate = 10;
// Width and height to use when resizing the image to make a descriptor for it
const int resizeW = 16, resizeH = 16;

// Converts the given video file to a descriptor of it (in this case a vector of resized and grayscaled frames)
// returns the vector with the converted frames, total frames the original video has and its duration in ms.
std::tuple<std::vector<Mat>, double, long double> videoToDescriptor(const String &videoPath, bool verbose = false) {
    VideoCapture cap(videoPath);
    // TODO: Add error handling for when the capture fails.
    double totalFrames = cap.get(CAP_PROP_FRAME_COUNT);
    // Amount of frames we are going to use from the video (so we can do the following fixed size initialization)
    auto sampledLength = (unsigned long) (((totalFrames - 1)/ sampleRate) + 1);
    // Vector where we'll store all converted frames
    std::vector<Mat> convertedFrames(sampledLength);
    if(verbose) {
        std::cout << "Processing " << videoPath << ". Total frames: " << totalFrames <<
                  ". Sampled frames: " << sampledLength << std::flush;
    }
    unsigned long cfIndex = 0;
    // Sampling frames from the video and converting them into a more descriptor(ish) form:
    for(unsigned long i = 0; i < totalFrames; i++) {
        Mat original, converted;
        cap.grab();
        // Retrieve only every sampleRate (10) frames (starting with the first)
        if (i % sampleRate != 0) { continue; }
        cap.retrieve(original);
        cvtColor(original, converted, COLOR_BGR2GRAY);
        resize(converted, converted, Size(resizeW, resizeH), 0, 0, INTER_CUBIC);
        // Is 'converted' doing pointer stuff that's gonna mean all convertedFrames are the same in the end ;-;?
        convertedFrames[cfIndex++] = converted;
        // cfIndex to actual index: cfIndex*sampleRate + 1
    }
    long double duration;
    // Go to after the last frame.
    cap.set(CAP_PROP_POS_AVI_RATIO, 1);
    duration = cap.get(CAP_PROP_POS_MSEC);
    if(verbose) {std::cout << ". Duration (ms): " << duration << std::endl;}
    cap.release();
    return std::make_tuple(convertedFrames, totalFrames, duration);
}



// Takes all video files in the given input folder, with the given extension, and writes files with descriptors
// for each of them in the given output folder. It summarizes all the information in a directory file
int makeVideoDescriptorFiles (const String &inputFolder, const String &extension,
        const String &outputFolder, const String &directoryFolder) {
    std::vector<String> videoPaths;
    String pattern = inputFolder + "/*." + extension;
    // Assumes all relevant files are at the first level of the given directory, so set recursion to false.
    glob(pattern, videoPaths, false);
    // Directory of ads and their very general info:
    String directoryPath = directoryFolder + "/AdsDirectory";
    std::ofstream directoryFile(directoryPath);
    for(const String &videoPath: videoPaths) {
        String videoName = extractNameFromPath(videoPath);
        std::vector<Mat> convertedFrames;
        double totalFrames;
        long double duration;
        std::tie(convertedFrames, totalFrames, duration) = videoToDescriptor(videoPath, true);
        // Save info in directory (to be used at the end of the whole process)
        directoryFile << videoName << ' ' << totalFrames << ' ' << duration << '\n';
        unsigned long sampledLength = convertedFrames.size();
        // convertedFrames now holds all of every 10 frames from the video, in grayscale in resized to 10,10
        std::cout << "\tSampled and converted frames. Now saving..." << std::endl;
        String outFilePath = outputFolder + "/" + videoName  + ".txt";
        std::ofstream outFile(outFilePath);
        // First line is "[number of used frames] [number of frames in the original video] [duration]"
        outFile << sampledLength << ' ' << totalFrames << ' ' << duration << '\n';
        for(unsigned long i = 0; i < sampledLength; i++) {
            Mat current = convertedFrames[i];
            // Each row is the space separated pixel values of the frame
            // outFile << i;
            for(int row = 0; row < current.rows; row++){
                for(int col = 0; col < current.cols; col++) {
                    outFile << (int)current.at<uchar>(row, col);
                    if(!(row == (current.rows-1) && col== (current.cols -1))){
                        // Add as space as separate, except if we reached the end.
                        outFile << ' ';
                    }
                }
            }
            outFile << '\n';
        }
        outFile.close();
        if(!outFile) {
            std::cerr << "ERROR: Couldn't save descriptors for " << videoName << std::endl;
            return -1;
        }
        std::cout << "\tDescriptors saved in:\n\t\t" << outFilePath  << "\n\n";
    }
    std::cout << "All videos have been processed \\o/" << '\n';
    directoryFile.close();
    std::cout << "Directory of processed videos saved in:\n\t"
              << directoryPath << "\n\t(Format is: [name] [total frames] [duration])" << std::endl;
    return 1;
}
// A tuple represting the info in video's file of descriptors:
//  <2D vect and each inner vect has the intensities of one of the sampled frames, total frames in original, duration>
typedef std::tuple<std::vector<std::vector<int>>, long, double> DescriptorContainer;

// Read file with descriptors, return a vector of frame descriptors of the form
DescriptorContainer readDescriptors(const String &filePath) {
    // Thaanks https://stackoverflow.com/a/9571913/4192226
    std::ifstream file(filePath);
    std::string line;
    // Read header line
    std::getline(file, line);
    unsigned long sampledFrames; long totalFrames; double duration;
    std::stringstream lineStream(line);
    lineStream >> sampledFrames >> totalFrames >> duration;
    // Reading frames:
    unsigned long frameSize = resizeW*resizeH;
    std::vector<std::vector<int>> adFrames(sampledFrames, std::vector<int>(frameSize));
    for(unsigned long currentFrame = 0; currentFrame < sampledFrames; currentFrame ++){
        std::getline(file, line);
        lineStream = std::stringstream(line);
        int value;
        // Read an integer at a time from the line
        for(unsigned long i = 0; i < frameSize; i ++){
            lineStream >> value;
            adFrames[currentFrame][i] = value;
        }
    }
    return std::make_tuple(adFrames, totalFrames, duration);
}

// Represents the description of a single nearest frame:
//     <name of the ad, index of the frame (relative to sampled frames)>
// (total sampled frames are not recorded per ad frame because that would become very redundant information)
typedef std::tuple<String, unsigned long> NearestContainer;

void saveNearestVectToFile(std::vector<NearestContainer> &nearestVect, std::ofstream outFile) {
    // TODO: Make structs you tuple maniac. Access is annoying and implies memorization with the .get<n> thing.
    // TODO: Make it save/know the associations between adName, duration and total sampled frames.
    for(unsigned long i = 0; i < nearestVect.size(); i++) {
        outFile << std::get<0>(nearestVect[i]) << ' ' << std::get<1>(nearestVect[i]);
        outFile << '\n';
    }
    // outFile.close();
    // if(!outFile) {
    //     std::cerr << "ERROR: Couldn't save descriptors for " << videoName << std::endl;
    //     return -1;
    // }
    // std::cout << "\tDescriptors saved in:\n\t\t" << file  << "\n\n";
}

// for every frame of the given video, figures out the nearest frame from all ads to that frame of the video.
int findNearestFrames(const String &videoPath, const String &outputFolder, const String &adsDescFolder) {
    // Getting all ad descriptor paths
    std::vector<String> adDescPaths;
    String pattern = adsDescFolder + "/*.txt";
    glob(pattern, adDescPaths, false);

    // Vector of Mats representing a frame that's been converted into a descriptor (Gray, resized)
    std::vector<Mat> videoDescriptors;
    double totalFrames;
    long double duration;
    std::tie(videoDescriptors, totalFrames, duration) = videoToDescriptor(videoPath, true);

    // Let's bring all ad descriptors to memory (the alternative is reading each file for each video frame ^ ^'
    unsigned long totalAds = adDescPaths.size();
    std::vector<DescriptorContainer> allAdDescriptors(totalAds);
    std::vector<String> adNames(totalAds);
    // std::vector<double> allAdDurations(totalAds);
    for(int i = 0; i < totalAds; i++) {
        String descPath = adDescPaths[i];
        allAdDescriptors[i] = readDescriptors(descPath);
        adNames[i] = extractNameFromPath(descPath);
    }
    // Each nearest neighbor is described by:
    //     [dist, name of ad, frame index] (index is relative to sampled frames)
    // TODO How about two vectors/arrays instead of using tuples/pairs?
    std::vector<std::tuple<String, unsigned long>> nearestFrames(videoDescriptors.size());
    // Let's start iterating over all the converted frames of the video:
    std::cout << "Finding nearest frames!\n";
    for(unsigned long videoFrameIndex = 0; videoFrameIndex < videoDescriptors.size(); videoFrameIndex++) {
        if(videoFrameIndex%100 == 0) {
            std::cout << "Current sampled video frame: " << (videoFrameIndex+1) << std::endl;
        }
        // 214748347 is the max long (so the max possible distance)
        long currentBestDist = 214748347;
        // iterate over all ads:
        for(unsigned long adInd = 0; adInd < totalAds; adInd++) {
            DescriptorContainer currentAd = allAdDescriptors[adInd];
            std::vector<std::vector<int>> currentAdFrames = std::get<0>(currentAd);
            // and iterate over each one of the ad's converted frames:
            for(unsigned long adFrameInd = 0; adFrameInd < currentAdFrames.size(); adFrameInd++) {
                long dist = matVectEuclideanDist(videoDescriptors[videoFrameIndex], currentAdFrames[adFrameInd]);
                if(dist < currentBestDist) {
                    // If the current frame is a new nearest, save the distance, and a description of that nearest frame
                    nearestFrames[videoFrameIndex] = std::make_tuple(adNames[adInd], adFrameInd);
                    currentBestDist = dist;
                    // (continuationFrame: Sometimes consecutive frames look (pretty much) the same, this in an attempt
                    // to capture the right frame number even in the case when the right frame is == the previous one
                }
            }
        }
    }
    String videoName = extractNameFromPath(videoPath);
    String outFilePath = outputFolder + "/" + videoName  + ".txt";
    std::cout << "Saving nearest frames' information to:\n\t"
              << outFilePath << std::endl;

    std::ofstream outFile(outFilePath);
    // Header contains the long video's general info (total number of frames, and duration)
    outFile << totalFrames << ' ' << duration << '\n';
    // A second header which has the sampling rate and the W and H values used in the resize
    outFile << sampleRate << ' ' << resizeW << ' ' << resizeH << '\n';
    /** TODO Consider writing to the file at the end of each of the outermost for-loops up ^there, so we don't do two
        loops over the entirety of the sampled frames of the long video.**/
    // (not sure if keeping the file open while doing all that other stuff is gonna mean some kind of performance hit)
    // Now, for each frame of the video, record the nearest frame:
    for(NearestContainer &nearest: nearestFrames){
        // since the name has spaces, it's less problematic to save the name in its own line.
        outFile << std::get<0>(nearest) << '\n' << std::get<1>(nearest) << '\n';
    }
    outFile.close();
    std::cout << "Done saving nearest frames!" << std::endl;

}

int main() {
    // TODO: When it all comes together, make the descriptor generation optional via a cin argument or something.
    makeVideoDescriptorFiles("../ads", "mpg", "../ad-descriptors", "..");
    findNearestFrames("../tv/mega1.mp4", "..", "../ad-descriptors");
//    std::vector<String> videoPaths;
//    String pattern = "../ads/*.mpg";
//    // Assumes all relevant files are at the first level of the given directory, so set recursion to false.
//    glob(pattern, videoPaths, false);
//    VideoCapture cap("../ads/ballerina.mpg");
//    std::cout << "Frames! " << cap.get(cv::CAP_PROP_FRAME_COUNT) << std::endl;
// //    cap.set(CV_CAP_PROP_FRAME_WIDTH, 640);
// //    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 360);
//    Mat image, gimage;
// //    cap.read(image);
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
