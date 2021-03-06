/*
* YoloObjectDetector.h
*
*  Created on: Dec 19, 2016
*      Author: Marko Bjelonic
*   Institute: ETH Zurich, Robotic Systems Lab
*/

#pragma once

// c++
#include <math.h>
#include <string>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <chrono>

// ROS
#include <ros/ros.h>
#include <std_msgs/Header.h>
#include <std_msgs/Int8.h>
#include <actionlib/server/simple_action_server.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/Image.h>
#include <geometry_msgs/Point.h>
#include <image_transport/image_transport.h>

#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>

// OpenCv
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <cv_bridge/cv_bridge.h>

// darknet_ros_msgs
#include <darknet_ros_msgs/BoundingBoxes.h>
#include <darknet_ros_msgs/BoundingBox.h>
#include <darknet_ros_msgs/CheckForObjectsAction.h>

// object msgs
#include <tuw_object_msgs/ObjectDetection.h>

#include <rwth_perception_people_msgs/GroundPlane.h>

// Darknet.
#ifdef GPU
#include "cuda_runtime.h"
#include "curand.h"
#include "cublas_v2.h"
#endif

// Eigen3
#include <eigen3/Eigen/Dense>

extern "C" {
#include "network.h"
#include "detection_layer.h"
#include "region_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"
#include "darknet_ros/image_interface.h"
#include <sys/time.h>
}

extern "C" void ipl_into_image(IplImage *src, image im);
extern "C" image ipl_to_image(IplImage *src);
extern "C" void show_image_cv(image p, const char *name, IplImage *disp);

namespace darknet_ros
{
//! Bounding box of the detected object.
typedef struct
{
  float x, y, w, h, prob;
  int num, Class;
} RosBox_;

class YoloObjectDetector
{
public:
  /*!
   * Constructor.
   */
  explicit YoloObjectDetector(ros::NodeHandle nh);

  /*!
   * Destructor.
   */
  ~YoloObjectDetector();

private:
  /*!
   * Reads and verifies the ROS parameters.
   * @return true if successful.
   */
  bool readParameters();

  /*!
   * Initialize the ROS connections.
   */
  void init();

  /*!
   * Callback of camera.
   * @param[in] msg image pointer.
   */
  // void cameraCallback(const sensor_msgs::ImageConstPtr& msg);
  void cameraCallback(const sensor_msgs::ImageConstPtr &color_image, const sensor_msgs::ImageConstPtr &depth_image,
                      const sensor_msgs::CameraInfoConstPtr &camera_info);

  void groundPlaneCallback(const rwth_perception_people_msgs::GroundPlane::ConstPtr& gp);
  
  /*!
   * Check for objects action goal callback.
   */
  void checkForObjectsActionGoalCB();

  /*!
   * Check for objects action preempt callback.
   */
  void checkForObjectsActionPreemptCB();

  /*!
   * Check if a preempt for the check for objects action has been requested.
   * @return false if preempt has been requested or inactive.
   */
  bool isCheckingForObjects() const;

  /*!
   * Publishes the detection image.
   * @return true if successful.
   */
  bool publishDetectionImage(const cv::Mat &detectionImage);

  //! Typedefs.
  typedef actionlib::SimpleActionServer<darknet_ros_msgs::CheckForObjectsAction> CheckForObjectsActionServer;
  typedef std::shared_ptr<CheckForObjectsActionServer> CheckForObjectsActionServerPtr;
  
  rwth_perception_people_msgs::GroundPlane::ConstPtr gp_;
  Eigen::Vector3f gpn_;
  float gpd_;

  //! ROS node handle.
  ros::NodeHandle nodeHandle_;

  //! Class labels.
  int numClasses_;
  std::vector<std::string> classLabels_;

  //! Check for objects action server.
  CheckForObjectsActionServerPtr checkForObjectsActionServer_;

  //! Advertise and subscribe to image topics.
  image_transport::ImageTransport imageTransport_;

  //! ROS subscriber and publisher.
  image_transport::Subscriber imageSubscriber_;
  ros::Publisher objectPublisher_;
  ros::Publisher boundingBoxesPublisher_;
  ros::Publisher objectDetectionPublisher_;
  ros::Subscriber gpSubscriber_;

  std::unique_ptr<message_filters::Subscriber<sensor_msgs::CameraInfo>> cameraInfoSubscriber_;
  std::unique_ptr<message_filters::Subscriber<sensor_msgs::Image>> depthImageSubscriber_;
  std::unique_ptr<message_filters::Subscriber<sensor_msgs::Image>> colorImageSubscriber_;

  typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::Image, sensor_msgs::Image,
                                                          sensor_msgs::CameraInfo> syncPolicyImage;
  typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::Image,
                                                          sensor_msgs::CameraInfo> syncPolicyImageMonocular;

  std::unique_ptr<message_filters::Synchronizer<syncPolicyImage>> syncImage_;
  std::unique_ptr<message_filters::Synchronizer<syncPolicyImageMonocular>> syncImageMonocular_;
  
  // ros param
  bool monocular_;

  //! Detected objects.
  std::vector<std::vector<RosBox_>> rosBoxes_;
  std::vector<int> rosBoxCounter_;
  darknet_ros_msgs::BoundingBoxes boundingBoxesResults_;

  //! Camera related parameters.
  int frameWidth_;
  int frameHeight_;

  //! Publisher of the bounding box image.
  ros::Publisher detectionImagePublisher_;

  // Yolo running on thread.
  std::thread yoloThread_;

  // Darknet.
  char **demoNames_;
  image **demoAlphabet_;
  int demoClasses_;

  network *net_;
  image buff_[3];
  image buffLetter_[3];
  int buffId_[3];
  int buffIndex_ = 0;
  IplImage *ipl_;
  float fps_ = 0;
  float demoThresh_ = 0;
  float demoHier_ = .5;
  int running_ = 0;

  int demoDelay_ = 0;
  int demoFrame_ = 3;
  float **predictions_;
  int demoIndex_ = 0;
  int demoDone_ = 0;
  float *lastAvg2_;
  float *lastAvg_;
  float *avg_;
  int demoTotal_ = 0;
  double demoTime_;

  RosBox_ *roiBoxes_;
  bool viewImage_;
  bool enableConsoleOutput_;
  int waitKeyDelay_;
  int fullScreen_;
  char *demoPrefix_;

  std_msgs::Header imageHeader_;
  cv::Mat camImageCopy_;
  cv::Mat camImageDepthCopy_;
  Eigen::Matrix<float, 3, 3, Eigen::RowMajor> K_inv_;
  boost::shared_mutex mutexImageCallback_;

  bool imageStatus_ = false;
  boost::shared_mutex mutexImageStatus_;

  bool isNodeRunning_ = true;
  boost::shared_mutex mutexNodeStatus_;

  int actionId_;
  boost::shared_mutex mutexActionStatus_;

  // double getWallTime();

  int sizeNetwork(network *net);

  void rememberNetwork(network *net);

  detection *avgPredictions(network *net, int *nboxes);

  void *detectInThread();

  void *fetchInThread();

  void *displayInThread(void *ptr);

  void *displayLoop(void *ptr);

  void *detectLoop(void *ptr);

  void setupNetwork(char *cfgfile, char *weightfile, char *datafile, float thresh, char **names, int classes, int delay,
                    char *prefix, int avg_frames, float hier, int w, int h, int frames, int fullscreen);

  void yolo();

  IplImage *getIplImage();

  bool getImageStatus(void);

  bool isNodeRunning(void);

  void *publishInThread();
};

} /* namespace darknet_ros*/
