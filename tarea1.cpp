#include <opencv2/opencv.hpp>
#include "utils.h"
#include "config.cpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <tuple>
#include <algorithm>
ConfigContainer Config;
using namespace cv;

// Every how many frames are we going to get a frame to make a descriptor?
const int sampleRate = Config.sampleRate;
// Width and height to use when resizing the image to make a descriptor for it
const int resizeW = Config.resizeW, resizeH = Config.resizeH;

// Converts the given video file to a descriptor of it (in this case a vector of resized and grayscaled frames)
// returns the vector with the converted frames, total frames the original video has and its duration in ms.
std::tuple<std::vector<Mat>, double, long double> videoToDescriptor(const String &videoPath, bool verbose = false) {
    VideoCapture cap(videoPath);
    auto totalFrames = (unsigned long)cap.get(CAP_PROP_FRAME_COUNT);   // gives them as double
    // Amount of frames we are going to use from the video (so we can do the following fixed size initialization)
    unsigned long sampledLength = amountSampled(totalFrames, sampleRate);
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
        const String &descsOutputFolder, const String &directoryPath, bool verbose=false) {
    std::vector<String> videoPaths;
    String pattern = inputFolder + "/*." + extension;
    // Assumes all relevant files are at the first level of the given directory, so set recursion to false.
    glob(pattern, videoPaths, false);
    // Directory of ads and their very general info:
    std::ofstream directoryFile(directoryPath);
    for(const String &videoPath: videoPaths) {
        String videoName = extractNameFromPath(videoPath);
        std::vector<Mat> convertedFrames;
        double totalFrames;
        long double duration;
        std::tie(convertedFrames, totalFrames, duration) = videoToDescriptor(videoPath, true);
        // Save info in directory (to be used at the end of the whole process)
        directoryFile << videoName << '\n' << totalFrames << ' ' << duration << '\n';
        unsigned long sampledLength = convertedFrames.size();
        // convertedFrames now holds all of every 10 frames from the video, in grayscale in resized to 10,10
        if(verbose) {std::cout << "\tSampled and converted frames. Now saving..." << std::endl;}
        String outFilePath = descsOutputFolder + "/" + videoName  + ".txt";
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
        if(verbose){std::cout << "\tDescriptors saved in:\n\t\t" << outFilePath  << "\n\n";}
    }
    directoryFile.close();
    if(verbose) {
        std::cout << "All videos have been processed \\o/" << '\n';
        std::cout << "Directory of processed videos saved in:\n\t"
                  << directoryPath << "\n\t(Format is: [name] [total frames] [duration])" << std::endl;
    }
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
    unsigned long frameSize = (unsigned long)resizeW*resizeH;
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
struct NearestInfo {
    std::string name;
    int frame;
    NearestInfo() {name=""; frame=0;}
    NearestInfo(std::string &n, int f) {name = n; frame = f;}
};

// For every frame of the given video, figures out the nearest frame from all ads to that frame of the video.
void findNearestFrames(const String &videoPath, const String &outFilePath,
        const String &adsDescFolder, bool verbose=false) {
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
    std::vector<std::string> adNames(totalAds);
    for(int i = 0; i < totalAds; i++) {
        String descPath = adDescPaths[i];
        allAdDescriptors[i] = readDescriptors(descPath);
        adNames[i] = extractNameFromPath(descPath);
    }
    std::vector<NearestInfo> nearestFrames(videoDescriptors.size());
    // Let's start iterating over all the converted frames of the video:
    if(verbose){std::cout << "Finding nearest frames!\n";}
    for(unsigned long videoFrameIndex = 0; videoFrameIndex < videoDescriptors.size(); videoFrameIndex++) {
        if(videoFrameIndex%100 == 0) {
            if(verbose){std::cout << "Current sampled video frame: " << (videoFrameIndex+1) << std::endl;}
        }
        // 214748347 is the max long (so the max possible distance)
        long currentBestDist = 214748347;
        // iterate over all ads:
        for(int adInd = 0; adInd < totalAds; adInd++) {
            DescriptorContainer currentAd = allAdDescriptors[adInd];
            std::vector<std::vector<int>> currentAdFrames = std::get<0>(currentAd);
            // and iterate over each one of the ad's converted frames:
            for(int adFrameInd = 0; adFrameInd < currentAdFrames.size(); adFrameInd++) {
                long dist = matVectEuclideanDist(videoDescriptors[videoFrameIndex], currentAdFrames[adFrameInd]);
                if(dist < currentBestDist) {
                    nearestFrames[videoFrameIndex] = NearestInfo(adNames[adInd], adFrameInd+1);
                    currentBestDist = dist;
                }
            }
        }
    }
    String videoName = extractNameFromPath(videoPath);
    if(verbose){ std::cout << "Saving nearest frames' information to:\n\t" << outFilePath << std::endl; }

    std::ofstream outFile(outFilePath);
    // First the title of the video:
    outFile << videoName << '\n';
    // Then a header which contains the long video's general info (total number of frames, and duration)
    outFile << totalFrames << ' ' << duration << '\n';
    // A second header which has the sampling rate and the W and H values used in the resize
    outFile << sampleRate << ' ' << resizeW << ' ' << resizeH << '\n';
    // Now, for each frame of the video, record the nearest frame:
    for(auto &nearest: nearestFrames){
        // since the name has spaces, it's less problematic to save the name in its own line.
        outFile << nearest.name << '\n' << nearest.frame << '\n';
    }
    outFile.close();
    if(verbose){std::cout << "Done saving nearest frames!" << std::endl;}
}


// Oh hey, I'm using structs now! These are gonna be basically ad directory entries (except in memory instead of a file)
struct VideoInfo {
    unsigned long totalFrames;
    double duration;
    unsigned long totalSampled;
    VideoInfo() {totalFrames = 0; duration = 0; totalSampled = 0;}
    VideoInfo(unsigned long t, double d) {
        totalFrames = t;
        duration = d;
        totalSampled = amountSampled(totalFrames, sampleRate);
    }
};

std::unordered_map<std::string, VideoInfo> readAdDirectory(const String &adDirectoryPath) {
    std::unordered_map<std::string, VideoInfo> adDirectory;  // smh, cv::String doesn't hash!?
    std::ifstream file(adDirectoryPath);
    std::string numbers;
    std::string name;
    // Getting the directory with ad info
    while(std::getline(file, name)) {
        std::getline(file, numbers);
        std::stringstream lineStream(numbers);
        lineStream = std::stringstream(numbers);
        unsigned long totalFrames; double duration;
        lineStream >> totalFrames >> duration;
        adDirectory[name] = VideoInfo(totalFrames, duration);
    }
    file.close();
    return adDirectory;
}

// Used to track possible matches of a video inside the long video.
struct VideoMatchTracker {
    int nameMatches;
    int sequenceTracker;
    double nameFailScore;
    double sequenceFailScore;
    int matchStart;
    bool matching;
    VideoMatchTracker() {
        matchStart = 0;
        nameMatches = 0;
        sequenceTracker = 0;
        nameFailScore = 0;
        sequenceFailScore = 0;
        matching = false;
    }
};




int detectAds(const String &nearestFramesFilePath, const String &adDirectoryPath, const String &outFilePath) {
    // Getting the ad directory
    std::unordered_map<std::string, VideoInfo> adDirectory = readAdDirectory(adDirectoryPath);
    std::string line;
    std::ifstream file(nearestFramesFilePath);
    // Title of the long video is...
    std::string videoName;
    std::getline(file, videoName);
    // Now for the headers:
    unsigned long totalFrames; double duration;
    std::getline(file, line);
    std::stringstream lineStream(line);
    lineStream >> totalFrames >> duration;
    // Admittedly, the second header was sort of added just on principle, since we do have them as globals too.
    int samplingRate, sampleW, sampleH;
    std::getline(file, line);
    lineStream = std::stringstream(line);
    lineStream >> samplingRate >> sampleW >> sampleH;
    double fps = totalFrames/(duration/1000.0);
    // Reading file of nearest frames
    unsigned long totalSampled = amountSampled(totalFrames, sampleRate);
    std::vector<NearestInfo> nearestFrames(totalSampled);
    std::string adName;
    for(int frameInd = 0; frameInd < totalSampled; frameInd++){
        std::getline(file, adName);
        std::getline(file, line);
        int adFrame = std::stoi(line);
        nearestFrames[frameInd] = NearestInfo(adName, adFrame);
    }
    file.close();
    std::ofstream outFile(outFilePath);
    // Finding matches. For every ad...
    for(auto &ad: adDirectory) {
        adName = ad.first;
        VideoInfo adInfo = ad.second;
        VideoMatchTracker tracker = VideoMatchTracker();
        // Next, for every (sampled) frame from the long video
        for(int frameInd = 0; frameInd < totalSampled; frameInd++){
            NearestInfo currentNearest = nearestFrames[frameInd];
            std::string currNearName = currentNearest.name;
            int currNearFrame = currentNearest.frame;
            if(currNearName == adName) {
                // If the name matched the name of the ad we're looking at, let's see if we are starting a match
                if(!tracker.matching && (currNearFrame < Config.matchStartErrorMargin)) {
                    tracker.matching = true;
                    // Start the sequence at whichever frame number of the ad is closes to the frame of the video
                    tracker.sequenceTracker = currNearFrame;
                    // Remember the index (relative to the long video) of this frame
                    tracker.matchStart = frameInd;

                }
                else if(tracker.matching) {
                    // If we're already in the process of matching, let's track our progress
                    if(tracker.sequenceTracker <= currNearFrame){
                        double nForgive = Config.nameFailForgiveness;
                        // If the current nearest frame says it's a frame number bigger than where the sequence was
                        // then let's decrease the name fail score a bit, since this is a good match.
                        tracker.nameFailScore = std::max(tracker.nameFailScore-nForgive, (double)0);
                        if(currNearFrame > (tracker.sequenceTracker + 5)) {
                            // But the current nearest says it's a frame too far from our sequence, that may be bad
                            double penalty =  ((currNearFrame-tracker.sequenceTracker)*Config.sequenceOvershootFactor);
                            tracker.sequenceFailScore += penalty;
                        }
                        else{
                            double sForgive = Config.sequenceFailForgiveness;
                            // If not, then the sequence is going well, let's forgive a bit of its past mistakes
                            tracker.sequenceFailScore = std::max(tracker.sequenceFailScore - sForgive, (double) 0);
                        }
                        // Since we're matching and the nearest frame number did go up, increase the sequence by one
                        tracker.sequenceTracker +=1;
                    }
                    else {
                        // The current nearest frame says its from a point before where we are in the sequence
                        tracker.sequenceFailScore+=Config.sequenceUndershootPenalty;
                    }
                }
            } //endif(currNearName == adName)
            else {
                // The name of the nearest frame didn't match the ad we're looking up
                if(tracker.matching) {
                    // So if we were doing a match, increase the name fail score
                    tracker.nameFailScore += 1;
                }
            }
            bool failLimitReached = (tracker.nameFailScore > Config.nameFailLimit ||
                                     tracker.sequenceFailScore > Config.sequenceFailLimit);
            // Evaluate: Are we really still matching an ad? If there's too many fails, then prolly not really.
            if(tracker.matching && failLimitReached){
                // Reset the tracker
                tracker = VideoMatchTracker();
            }
            // error margin (-5): If we've matched this much, might as well count it at this point.
            if(tracker.sequenceTracker > (adInfo.totalSampled - Config.matchEndErrorMargin)) {
                // Notice this is zero-indexed, to get the moment the frame *starts* (so it could be 0)
                int actualFrameIndex = tracker.matchStart*samplingRate;
                double startTime = ((double)actualFrameIndex/fps);
                // We're specifically matching for entire ads (and some of these conditions reflect that)
                // so might as well use 'duration'
                outFile << videoName << '\t' << startTime << '\t'
                        << adInfo.duration/1000.0 << '\t' << adName << std::endl;
                tracker = VideoMatchTracker();
            }
        }
    }
    outFile.close();

}


int main() {
    // TODO: When it all comes together, make the descriptor generation optional via a cin argument or something.
    std::string longVideoPath;
    std::string adsFolder;
    std::cout << "Porfavor ingrese la ruta al video a procesar\n"
                 "\tejemplo, relativo al ejecutable:\n"
                 "\t../television/mega-2014_04_10.mp4"<< std::endl;
    std::getline(std::cin, longVideoPath);
    std:: cout << "Porfavor ingrese la ruta a la carpeta donde están los comerciales. Por ejemplo:\n"
                  "\t../comerciales" << std::endl;
    std::getline(std::cin, adsFolder);

    
    makeVideoDescriptorFiles("../ads", "mpg", "../ad-descriptors", "../AdsDirectory");
    findNearestFrames("../tv/mega-2014_04_25.mp4", "../mega-2014_04_25.txt", "../ad-descriptors");
    detectAds("../mega-2014_04_10.txt", "../AdsDirectory", "../results.txt");
}
