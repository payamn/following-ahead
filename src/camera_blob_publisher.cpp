#define CAM_WRT_LASER_X -0.075
#define CAM_WRT_LASER_Y -0.01
#define CAM_WRT_LASER_Z 0.174

#define CAM_WRT_LASER_PITCH 16.5

#include "ros/ros.h"
#include "yolo2/ImageDetections.h"
#include <std_msgs/Bool.h>
#include <std_msgs/String.h>
#include <sensor_msgs/Image.h>
#include <message_filters/subscriber.h>
#include <geometry_msgs/Twist.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <image_transport/image_transport.h>
#include <image_transport/subscriber_filter.h>
#include <cv_bridge/cv_bridge.h>
#include <pcl_ros/point_cloud.h>
#include <pcl/point_types.h>
#include <sensor_msgs/PointCloud.h>
#include <geometry_msgs/Point32.h>
#include <geometry_msgs/TransformStamped.h>
#include "robot.hpp"
#include "utils.hpp"

#include <sstream>

#include <limits> 
#include <vector> 
#include <iostream> 
#include <cassert>
#include <algorithm>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>

class CameraBlobPublisher
{
private:
  ros::Publisher pubPointCloud_;
  ros::Publisher pubRelativePose_;
  sensor_msgs::PointCloud::Ptr pointCloudMsg_;
  ros::NodeHandle nh_;
  tf::TransformBroadcaster tf_broadcaster_;
  cv::Point3f human_prev_pose;
  float camera_wrt_laser_x_;
  float camera_wrt_laser_y_;
  float camera_wrt_laser_z_;
  float camera_wrt_laser_pitch_;

  std::string camera_parent_frame_;

public:
  CameraBlobPublisher(ros::NodeHandle n)
    : nh_(n),
      pointCloudMsg_(new sensor_msgs::PointCloud)
  {
    pubPointCloud_ =  nh_.advertise<sensor_msgs::PointCloud>("person_cloud", 1);
    pubRelativePose_ = nh_.advertise<geometry_msgs::TransformStamped>("/person_follower/groundtruth_pose", 1);
  
    if (!nh_.getParam("camera_wrt_laser_x", camera_wrt_laser_x_))
    {
      camera_wrt_laser_x_ = CAM_WRT_LASER_X;
    }

    if (!nh_.getParam("camera_wrt_laser_y", camera_wrt_laser_y_))
    {
      camera_wrt_laser_y_ = CAM_WRT_LASER_Y;
    }

    if (!nh_.getParam("camera_wrt_laser_z", camera_wrt_laser_z_))
    {
      camera_wrt_laser_z_ = CAM_WRT_LASER_Z;
    }

    if (!nh_.getParam("camera_wrt_laser_pitch", camera_wrt_laser_pitch_))
    {
      camera_wrt_laser_pitch_ = CAM_WRT_LASER_PITCH;
    }

    if (!nh_.getParam("camera_parent_frame", camera_parent_frame_))
    {
      camera_parent_frame_ = "laser";
    }

  }

  void detectionCallback(const yolo2::ImageDetections::ConstPtr &detectionMsg, const sensor_msgs::Image::ConstPtr &depthMsg)
  {

    std::vector<cv::Point2f> undistortedPoints;
    std::vector<float> depthValues;
    std::vector<cv::Point2f> vectPersonPoints;
    
    cv_bridge::CvImageConstPtr cv_ptr;
    cv_ptr = cv_bridge::toCvShare(depthMsg);

    // cv::Mat matPersonSegmented = cv::Mat::zeros(depthMsg->height, depthMsg->width, CV_8UC1);
    
    for (int i=0; i<detectionMsg->detections.size(); i++)
    {
      if (detectionMsg->detections[i].class_id != 0)
      {
        continue;
      }

      if (vectPersonPoints.size())
      {
        // another person, don't care
        break;
      }
      
      // Stg::ModelBlobfinder  myFinder;
      // std::cout << detectionMsg->detections[i].class_id << std::endl;
      // std::cout << detectionMsg->detections[i].x << ", " << detectionMsg->detections[i].y << std::endl;
      // std::cout << detectionMsg->detections[i].width << ", " << detectionMsg->detections[i].height << std::endl << std::endl  ;
      // // myFinder.fov = 70.0*M_PI/180.0;
      // myFinder.
    
      for (
            int row=detectionMsg->detections[i].roi.y_offset; 
            row<detectionMsg->detections[i].roi.y_offset+detectionMsg->detections[i].roi.height;
            row++
          )
      {
        if (row < 0 || row >= cv_ptr->image.rows)
        {
          continue;
        }
        for (
              int col=detectionMsg->detections[i].roi.x_offset; 
              col<detectionMsg->detections[i].roi.x_offset+detectionMsg->detections[i].roi.width;
              col++
            )
        {
          if (col < 0 || col >= cv_ptr->image.cols)
          {
            continue;
          }

          float depth = ((float)cv_ptr->image.at<uint16_t>(row, col)) / 1000.0;
          if (depth < 0.01 || depth > 6.0 || !std::isfinite(depth))
          {
            continue;
          }

          vectPersonPoints.push_back(cv::Point2f(col, row));
          depthValues.push_back(depth);
        }
      }

      if (vectPersonPoints.size() == 0)
      {
        ROS_WARN("No point in person: %d", i);
        continue;
      }

      size_t medianIdx = depthValues.size()/2;
      std::nth_element(depthValues.begin(), depthValues.begin()+medianIdx, depthValues.end());
      float medianDepth = depthValues[medianIdx];

      std::vector<cv::Point2f> vectPersonPointsSegmented;
      std::vector<float> depthValuesSegmented;

      for (size_t i = 0; i < vectPersonPoints.size(); i++)        
      {
        int row = vectPersonPoints[i].y;
        int col = vectPersonPoints[i].x;

        float depth = ((float)cv_ptr->image.at<uint16_t>(row, col))/1000.0;
        if (
              depth >= medianDepth-0.12 &&
              depth <= medianDepth+0.12
            )   
        {
          vectPersonPointsSegmented.push_back(vectPersonPoints[i]);
          depthValuesSegmented.push_back(depthValues[i]);
        }
      }

      vectPersonPoints = vectPersonPointsSegmented;
      depthValues = depthValuesSegmented;
    }

    // cv::imshow("window", matPersonSegmented);
    // cv::waitKey(5);

    if (vectPersonPoints.size()==0)
    {
      ROS_WARN("No detection");
      return;
    }

    // cv::Mat cameraMatrix = cv::Mat::eye(3, 3, CV_32F);
    // cameraMatrix.at<float>(0, 0) = focalLengthX;
    // cameraMatrix.at<float>(1, 1) = focalLengthY;
    // cameraMatrix.at<float>(0, 2) = cameraPrincipalX;
    // cameraMatrix.at<float>(1, 2) = cameraPrincipalY;

    // cv::Mat newCameraMatrix(3, 3, CV_32F);
    
    // cv::Vec<float, 5> distortionCoeffs;
    
    // distortionCoeffs[0] = 0.07346920669078827;
    // distortionCoeffs[1] = -0.12585918605327606;
    // distortionCoeffs[2] = -0.0005786623223684728;
    // distortionCoeffs[3] = -0.0006174440495669842;
    // distortionCoeffs[4] = 0.000000;

    // undistortedPoints.clear();
    // cv::undistortPoints(vectPersonPoints, undistortedPoints, cameraMatrix, distortionCoeffs);
    
    pointCloudMsg_->header.frame_id = "camera";
    // pointCloudMsg_->height = pointCloudMsg_->width = 1;
    pointCloudMsg_->header.stamp = ros::Time::now();
    pointCloudMsg_->points.clear();

    for (size_t i = 0; i < vectPersonPoints.size(); i++)
    {
      // cv::Point2f pt = undistortedPoints[i];
      float depth = cv_ptr->image.at<uint16_t>( (size_t)vectPersonPoints[i].y, (size_t)vectPersonPoints[i].x ) / 1000.0;

      float x = (vectPersonPoints[i].x - cameraPrincipalX)/focalLengthX*depth;
      float y = (vectPersonPoints[i].y - cameraPrincipalY)/focalLengthY*depth;
      float z = depth;

      geometry_msgs::Point32 point;
      // point.x = -x;
      // point.y = -z;
      // point.z = -y;

      point.x = x;
      point.y = y;
      point.z = z;

      pointCloudMsg_->points.push_back(point);
    }
    pubPointCloud_.publish(pointCloudMsg_);

    // find the center of the person
    int max_x = -1;
    int min_x = cv_ptr->image.cols + 1;
    
    for (size_t i = 0; i < vectPersonPoints.size(); i++)
    {
      if (vectPersonPoints[i].x > max_x)
      {
        max_x = vectPersonPoints[i].x;
      }

      if (vectPersonPoints[i].x < min_x)
      {
        min_x = vectPersonPoints[i].x;
      }
    }

    size_t medianIdx = depthValues.size()/2;
    std::nth_element(depthValues.begin(), depthValues.begin()+medianIdx, depthValues.end());
    float person_z = depthValues[medianIdx];

    float person_x = (min_x + max_x) / 2.0;
    person_x = (person_x - cameraPrincipalX)/focalLengthX*person_z;
    float person_y = 0;

    geometry_msgs::TransformStamped transform_stamped;
    transform_stamped.header.stamp = ros::Time::now();
    transform_stamped.header.frame_id = "camera";
    transform_stamped.transform.translation.x = person_x;
    transform_stamped.transform.translation.y = person_y;
    transform_stamped.transform.translation.z = person_z;

    transform_stamped.transform.rotation.x = 0;
    transform_stamped.transform.rotation.y = 0;
    transform_stamped.transform.rotation.z = 0;
    transform_stamped.transform.rotation.w = 1;

    pubRelativePose_.publish(transform_stamped);

    tf::StampedTransform relative_tf;
    relative_tf.child_frame_id_ = "relative_pose"; // source
    relative_tf.frame_id_ = "camera"; // target
    relative_tf.stamp_ = transform_stamped.header.stamp;

    relative_tf.setOrigin(tf::Vector3( 
      transform_stamped.transform.translation.x, 
      transform_stamped.transform.translation.y, 
      transform_stamped.transform.translation.z
    ));

    relative_tf.setRotation(tf::Quaternion(0, 0, 0, 1));

    tf_broadcaster_.sendTransform(relative_tf);

    // ----------------------- broadcast camera tf wrt world ---------------------------------
    tf::StampedTransform camera_tf;
    camera_tf.child_frame_id_ = "camera"; // source
    camera_tf.frame_id_ = camera_parent_frame_; // target
    camera_tf.stamp_ = transform_stamped.header.stamp;

    camera_tf.setOrigin(tf::Vector3( 
      camera_wrt_laser_x_, 
      camera_wrt_laser_y_, 
      camera_wrt_laser_z_
    )); 

    float pitch_angle = camera_wrt_laser_pitch_ * M_PI / 180.;
    
    cv::Point3f basis_x(
      0, 1, 0
    );

    cv::Point3f basis_z(
      -cos(pitch_angle),
      0,
      sin(pitch_angle)
    );

    cv::Point3f basis_y = basis_z.cross(basis_x);
    
    tf::Quaternion q;
    tf::Matrix3x3 rotation(
      basis_x.x, basis_y.x, basis_z.x,
      basis_x.y, basis_y.y, basis_z.y,
      basis_x.z, basis_y.z, basis_z.z 
    );
    rotation.getRotation(q);
    camera_tf.setRotation(q);

    tf_broadcaster_.sendTransform(camera_tf);

  }
};

int main(int argc, char** argv)
{

  ros::init(argc, argv, "camera_blob_publisher"); 
  ros::NodeHandle n("~");

  CameraBlobPublisher camera_blob_publisher(n);
  image_transport::ImageTransport image_transporter(n);

  message_filters::Subscriber<yolo2::ImageDetections> detectionSub(n, "/vision/yolo2/detections", 1);
  image_transport::SubscriberFilter depthSub(image_transporter, "/camera/depth/image_raw", 1);
  
  typedef message_filters::sync_policies::ApproximateTime<yolo2::ImageDetections, sensor_msgs::Image> SyncPolicy;
  message_filters::Synchronizer<SyncPolicy> sync(SyncPolicy(1000), detectionSub, depthSub);
  sync.registerCallback(boost::bind(&CameraBlobPublisher::detectionCallback, &camera_blob_publisher, _1, _2));

  ros::spin();
  return 0;
}