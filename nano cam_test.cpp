#include <opencv2/opencv.hpp>
int main() {
    cv::VideoCapture cap(0);
    if(!cap.isOpened()) return -1;
    cv::Mat frame;
    while(true){
        cap >> frame;
        if(frame.empty()) break;
        cv::imshow("cam", frame);
        if(cv::waitKey(30) == 27) break; // ESC to exit
    }
    return 0;
}
