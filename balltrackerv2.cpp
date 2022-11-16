// ballTracker.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <math.h>

using namespace cv;
using namespace std;


int main()
{
    utils::logging::setLogLevel(utils::logging::LogLevel::LOG_LEVEL_SILENT); // removes ugly opencv info logging messsages
    cout << "Loading images...\n" << endl;

    //2 Mat objects hold original image and shifted image
    Mat orig;
    Mat shift;

    // read 2 images from file
    orig = imread("grid2/00.bmp", 0);
    shift = imread("grid2/25.bmp", 0);

    //Apply small gaussian blur to improve circle detection
    Mat orig_blur;
    Mat shift_blur;
    GaussianBlur(orig, orig_blur, Size(3, 3), 1, 1);
    GaussianBlur(shift, shift_blur, Size(3, 3), 1, 1);


    //Use hough circles to determine precise locations of balls
    vector<Vec3f> orig_circles; //  Vec3f object holds x,y,radius at indices 0,1,2 respectively
    vector<Vec3f> shift_circles; // Vector object holds each circle found as Vec3f 
    HoughCircles(orig_blur, orig_circles, HOUGH_GRADIENT, 1, orig_blur.rows / 16, 40, 50, 300, 700); // original image, vector array of circles, method of detection, 1, minimum distance between circle centers, 2 threshold values, min/max radius of circles
    HoughCircles(shift_blur, shift_circles, HOUGH_GRADIENT, 1, shift_blur.rows / 16, 40, 50, 300, 700); // alter threshold values, circle radii to best fit to image

    //if no circles break
    if (orig_circles.size() == 0 || shift_circles.size() == 0) {
        cout << "\n\nNo circles found --  *Check HoughCircles parameters*\n\n" << endl;
        return 0;
    }

    Mat drawing;
    cvtColor(orig, drawing, COLOR_GRAY2RGB);
    //draw circles on original
    for (size_t i = 0; i < orig_circles.size(); i++) {
        Vec3i c = orig_circles[i];
        Point center = Point(c[0], c[1]);
        //find center of circle and draw at radius
        circle(drawing, center, 2, Scalar(0, 0, 255), 3, LINE_AA);
        int radius = c[2];
        circle(drawing, center, radius, Scalar(0, 0, 255), 3, LINE_AA);
    }

    //imshow("detected circles base", orig_blur);

    //draw circles on shifted image
    imwrite("circles_orig.jpg", drawing);

    Mat drawing_shift;
    cvtColor(shift, drawing_shift, COLOR_GRAY2RGB);
    for (size_t i = 0; i < shift_circles.size(); i++) {
        Vec3i c = shift_circles[i];
        Point center = Point(c[0], c[1]);
        // find center of circle and draw at radius
        circle(drawing_shift, center, 2, Scalar(255, 0, 0), 3, LINE_AA);
        int radius = c[2];
        circle(drawing_shift, center, radius, Scalar(255, 0, 0), 3, LINE_AA);
    }
    //imshow("detected circles shift", shift_blur);
    imwrite("circles_shift.jpg", drawing_shift);

    float alpha = 0.5;
    float beta = (1.0 - alpha);
    Mat dst;
    addWeighted(drawing, alpha, drawing_shift, beta, 0.0, dst);
    arrowedLine(dst, Point(orig_circles[0][0], orig_circles[0][1]), Point(shift_circles[0][0], shift_circles[0][1]), Scalar(255,255,255),3);

    //imshow("merge",dst);
    imwrite("overlay.jpg", dst);

    // print shifts between circle center positions
    printf("\n\n\n\nORIG CENTER AT (%f,%f), SHIFT CENTER AT (%f,%f)\n", orig_circles[0][0], orig_circles[0][1], shift_circles[0][0], shift_circles[0][1]);


    //calculate distance to ball using exact ball dimensions
    float ball_radius = 0.0127/2;

    float scale_factor_orig = ball_radius / orig_circles[0][2];

    float scale_factor_shift = ball_radius / shift_circles[0][2];

    float distance = 0.25;

    float aovx = 2 * atan(5.7 / 100);
    float aovy = 2 * atan(4.28 / 100);

    float fovx = 2 * tan(aovx / 2) * distance;
    float fovy = 2 * tan(fovx / 2) * distance;

    float pixelx = fovx / 2592;
    float pixely = fovy / 1944;

    printf("pixelf %f pixely %f", pixelx*1e6,pixely*1e6);
    
    printf("\nSCALE IS %f and %f microns per pixel in x and y\n\n", scale_factor_orig *1e6, scale_factor_shift*1e6);

    // calculate the actual shift in x-y
    float x_shift = (scale_factor_orig * orig_circles[0][0]) - (scale_factor_orig * shift_circles[0][0]);
    float y_shift = (scale_factor_orig * orig_circles[0][1]) - (scale_factor_orig * shift_circles[0][1]);
    printf("height of object in image: %f", orig_circles[0][2]);

    printf("(CIRCLE) TOTAL SHIFT IN X: %f (microns) TOTAL SHIFT IN Y: %f (microns)\n\n\n\n", x_shift*1e6, y_shift*1e6);

    






    waitKey(0);




}
