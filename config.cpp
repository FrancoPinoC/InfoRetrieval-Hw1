#include <string>
// Lotsa configuration.

struct ConfigContainer {
    // Sampling and descriptor related
    const int sampleRate = 10;
    const int resizeW = 16, resizeH = 16;

    // Detection related
    const int matchStartErrorMargin = 5;
    const int matchEndErrorMargin = 5;
    const double nameFailLimit = 4;
    const double sequenceFailLimit = 10;
    const double sequenceOvershootFactor = 1.0/5.0;
    const double nameFailForgiveness = 0.2;
    const double sequenceFailForgiveness = 0.2;
    const double sequenceUndershootPenalty = 0.5;

    // Input and output related
    const std::string adExtension = "mpg";
    const std::string

};