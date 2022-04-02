// taken from https://github.com/opencv/opencv/blob/4.x/samples/dnn/object_detection.cpp#L5
// modified to not use queues or threads due to how we're using it

#include <fstream>
#include <sstream>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace cv;
using namespace dnn;

float confThreshold, nmsThreshold;
std::vector<std::string> classes;
std::vector<String> outNames; // check up on this variable

inline void
preprocess(const Mat &frame, Net &net, Size inpSize, float scale,
           const Scalar &mean, bool swapRB);

vector<int> postprocess(Mat &frame, const std::vector<Mat> &out, Net &net, int backend);

void drawPred(int classId, int left, int top, int right, int bottom, Mat &frame);

void callback(int pos, void *userdata);

Net loadInModel(std::string modelPath, std::string configPath, std::string framework);

std::vector<std::string> detect_object(Mat &frame, Net &net, int width, int height);