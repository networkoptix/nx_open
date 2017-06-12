/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include "opencv2/opencv.hpp"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc_c.h>

#include "Classifier.h"

using namespace std;
using namespace cv;

#include "opencv_consumer_interface.h"

struct opencv_sample_handler
{
    Classifier *classifier;

    volatile int detector_busy;
    volatile int result_update_flag;
    volatile int detector_quit_flag;

    pthread_t detector_thread;
    cv::Mat detecting_mat;
    std::vector<Prediction> predictions;
    string *model_file;
    string *trained_file;
    string *mean_file;
    string *label_file;

    int width;
    int height;

    char first_result[4096];
    float first_prob;
    char second_result[4096];
    float second_prob;
    long milliseconds;
};


static void
matPrint(Mat &img, int lineOffsY, Scalar fontColor, const string &ss)
{
    int fontFace = FONT_HERSHEY_DUPLEX;
    double fontScale = 1.;
    int fontThickness = 1;
    Size fontSize = cv::getTextSize("T[]", fontFace, fontScale, fontThickness, 0);

    Point org;
    org.x = 1;
    org.y = 3 * fontSize.height * (lineOffsY + 1) / 2;
    putText(img, ss, org, fontFace, fontScale, CV_RGB(0,0,0), 5*fontThickness/2, 16);
    putText(img, ss, org, fontFace, fontScale, fontColor, fontThickness, 16);

    return;
}

static void *
detector_thread_entry(void *arg)
{
    struct opencv_sample_handler *handler =
        (struct opencv_sample_handler *) arg;

    cv::Mat zero_img_buf =
        cv::Mat::zeros(handler->height, handler->width, CV_8U);

    handler->classifier = new Classifier(*handler->model_file
        , *handler->trained_file
        , *handler->mean_file
        , *handler->label_file);

    while (handler->detector_quit_flag == 0)
    {
        if (handler->detector_busy == 1)
        {
            cout << "To classify " << std::endl;
            struct timeval tp;
            gettimeofday(&tp, NULL);
            long start = tp.tv_sec * 1000 + tp.tv_usec / 1000;
            std::vector<Prediction> predictions =
                handler->classifier->Classify(handler->detecting_mat);
            gettimeofday(&tp, NULL);
            long end = tp.tv_sec * 1000 + tp.tv_usec / 1000;
            // Print the top N predictions.
            handler->result_update_flag = 0;
            for (size_t i = 0; i < predictions.size(); i++)
            {
                Prediction p = predictions[i];
                std::cout << std::fixed << std::setprecision(4)  << p.second
                    << " - \"" << p.first << "\"" << std::endl;
                if (i == 0)
                {
                    handler->first_prob = p.second;
                    sprintf(handler->first_result, "%s", p.first.c_str());
                }
                else if (i == 1)
                {
                    handler->second_prob = p.second;
                    sprintf(handler->second_result, "%s", p.first.c_str());
                }
            }
            handler->milliseconds = end - start;
            handler->detector_busy = 0;
            handler->result_update_flag = 1;
        }
    }

    pthread_exit(NULL);
}

extern "C" void
opencv_set_config(void *opencv_handler, int config_id, void *arg)
{
    struct opencv_sample_handler *handler =
        (struct opencv_sample_handler *) opencv_handler;

    cout << "opencv_set_config " << config_id << endl;

    switch (config_id)
    {
    case OPENCV_CONSUMER_CONFIG_IMGWIDTH:
        handler->width = * (int *) arg;
        cout << "Image width " << handler->width << endl;
        break;
    case OPENCV_CONSUMER_CONFIG_IMGHEIGHT:
        handler->height = * (int *) arg;
        cout << "Image height " << handler->height << endl;
        break;
    case OPENCV_CONSUMER_CONFIG_CAFFE_MODELFILE:
        handler->model_file = new std::string((char *) arg);
        cout << "CAFFE model file : " << handler->model_file << endl;
        break;
    case OPENCV_CONSUMER_CONFIG_CAFFE_TRAINEDFILE:
        handler->trained_file = new std::string((char *) arg);
        cout << "CAFFE trained file : " << handler->trained_file << endl;
        break;
    case OPENCV_CONSUMER_CONFIG_CAFFE_MEANFILE:
        handler->mean_file = new std::string((char *) arg);
        cout << "CAFFE mean file : " << handler->mean_file << endl;
        break;
    case OPENCV_CONSUMER_CONFIG_CAFFE_LABELFILE:
        handler->label_file = new std::string((char *) arg);
        cout << "CAFFE label file : " << handler->label_file << endl;
        break;
    case OPENCV_CONSUMER_CONFIG_START:
        pthread_create(&handler->detector_thread
            , NULL, detector_thread_entry, (void *)handler);
        break;
    default:
        cerr << "Unknown config id: " << config_id << endl;
        break;
    }

    return;
}

extern "C" void *
opencv_handler_open(void)
{
    cout << "opencv_handler_open is called" << endl;

    struct opencv_sample_handler *handler = (struct opencv_sample_handler *)
        malloc(sizeof(struct opencv_sample_handler));

    memset(handler, 0, sizeof(struct opencv_sample_handler));
    handler->detector_busy = 0;
    handler->result_update_flag = 0;
    handler->detector_quit_flag = 0;

    return handler;
}

extern "C" void
opencv_handler_close(void *opencv_handler)
{
    struct opencv_sample_handler *handler =
        (struct opencv_sample_handler *) opencv_handler;

    cout << "opencv_handler_close with " << handler << " is called" << endl;

    handler->detector_quit_flag = 1;
    pthread_join(handler->detector_thread, NULL);
    delete handler->model_file;
    delete handler->trained_file;
    delete handler->mean_file;
    delete handler->label_file;
    delete handler->classifier;

    free(handler);

    return;
}

extern "C" void
opencv_img_processing(void *opencv_handler, uint8_t *pdata, int32_t width, int32_t height)
{
    struct opencv_sample_handler *handler =
        (struct opencv_sample_handler *) opencv_handler;

    cv::Mat imgbuf = cv::Mat(height, width, CV_8UC4, pdata);
    cv::Mat display_img;

    if (handler->detector_busy == 0) {
        //handler->detecting_mat = imgbuf.clone();
        imgbuf.copyTo(handler->detecting_mat);
        handler->detector_busy = 1;
    }
    cvtColor(imgbuf, display_img, CV_RGBA2BGRA);
    if (handler->result_update_flag)
    {
        ostringstream ss;
        ss.str("");
        ss << "FPS = " << std::fixed << std::setprecision(0)
             << 1000. / handler->milliseconds;
        matPrint(display_img, 0, CV_RGB(255,0,0), ss.str());
        ss.str("");
        ss << std::fixed << std::setprecision(2) << handler->first_prob << " - " << handler->first_result;
        matPrint(display_img, 1, CV_RGB(255,0,0), ss.str());
        ss.str("");
        ss << std::fixed << std::setprecision(2) << handler->second_prob << " - " << handler->second_result;
        matPrint(display_img, 2, CV_RGB(255,0,0), ss.str());
    }
    cv::imshow("img", display_img);
    waitKey(1);
    return;
}
