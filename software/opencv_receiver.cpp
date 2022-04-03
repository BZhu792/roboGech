#include <curl/curl.h>

#include <iostream>
#include <vector>
using namespace std;

#include <opencv2/opencv.hpp>
#include "object_detection.h"

using namespace cv;

typedef enum
{
    PLASTIC_BIN = 0,
    PAPER_BIN = 1,
    CAN_BIN = 2,
    DEFAULT_BIN = 3,

} bin_t;

typedef enum
{
    LED_IDLE = 0,
    LED_PENDING = 1,
    LED_APPROVE = 2,
    LED_TIMEOUT = 3,

} led_status_t;

// typedef struct state_packet
// {
//     bin_t object_type;
//     led_status_t state;
// } packet;

const char *url = "http://172.20.10.2/cam-hi.jpg";

// helper functions

// taken from https://answers.opencv.org/question/91344/load-image-from-url/
// adds data to a stream that the Mat will later take to make into an image
size_t write_data(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    vector<uchar> *stream = (vector<uchar> *)userdata;
    size_t count = size * nmemb;
    stream->insert(stream->end(), ptr, ptr + count);
    return count;
}

// gets an image from the url
Mat curl_image(const char *url, int timeout = 10)
{
    vector<uchar> stream; // set up input stream
    // set up GET request
    CURL *curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    }

    CURLcode res = curl_easy_perform(curl); // do it
    curl_easy_cleanup(curl);
    return imdecode(stream, -1);
}

// sends data to url depending on object detection

// object_res is the result of the object detection
// led_res determines if the camera is certain it is object res
void sendStates(const char *url, bin_t object_res, led_status_t led_res)
{
    std::string obj_str = std::to_string(static_cast<int>(object_res));
    std::string led_str = std::to_string(static_cast<int>(led_res));
    CURL *curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);

        std::string post_fields = "bin=";
        post_fields.append(obj_str);
        post_fields.append("led=");
        post_fields.append(led_str);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            std::cout << "Failed to send data\n";
        }
        curl_easy_cleanup(curl);
    }
}

// main functions
void liveTransmission()
{
    namedWindow("Live transmission", WINDOW_AUTOSIZE);
    Mat img; // figure out where to put this
    while (true)
    {
        img = curl_image(url);
        if (img.empty())
        {
            std::cout << "Loading failed\n";
        }
        imshow("Live Transmission", img);
    }

    // figure out how to wait on specific key value
    waitKey(0);
    destroyAllWindows();
}

void detectObjects()
{
    namedWindow("Detection", WINDOW_AUTOSIZE);
    Net nn = loadInModel("model path", "config path", "framework?", "classnames somehow");

    Mat img;
    while (true)
    {
        img = curl_image(url);

        // get width, height of image and replace 100s with it
        // do some object detection shenanigans
        std::vector<std::string> results = detect_object(img, nn, 100, 100);

        // process results

        imshow("Detection", img);
        waitKey(0);
    }
    destroyAllWindows();
}

int main(int argc, char **argv)
{
    std::cout << "started\n";
    Mat image = curl_image(url);
    if (image.empty())
    {
        std::cout << "Loading failed\n";
    }
    namedWindow("Live transmission", WINDOW_AUTOSIZE);
    imshow("Live transmission", image); // if it all goes well
    waitKey(0);
    return 0;
}