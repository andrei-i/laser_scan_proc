#include <message_filters/subscriber.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/synchronizer.h>
#include <ros/console.h>
#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <tf/transform_listener.h>

#include <iterator>
#include <limits>

using namespace sensor_msgs;
using namespace message_filters;

ros::Publisher pub;

void callback(const LaserScanConstPtr& scan_1,
              const LaserScanConstPtr& scan_2) {
	float angle_modif = 0;
	/*tf::StampedTransform transform;
	ros::Time t = ros::Time(0);
	tf::TransformListener tf_;
	tf_.waitForTransform(scan_2->header.frame_id, scan_1->header.frame_id, t, ros::Duration(2.0));
	tf_.lookupTransform(scan_2->header.frame_id,scan_1->header.frame_id, t, transform);
	angle_modif = tf::getYaw(transform.getRotation());
	*/
  const LaserScanConstPtr& smaller =
      (scan_1->angle_increment <= scan_2->angle_increment) ? scan_1 : scan_2;
  const LaserScanConstPtr& larger =
      (scan_1->angle_increment <= scan_2->angle_increment) ? scan_2 : scan_1;
  LaserScan merged;

  // Copy over header and basic information from scan_1
  merged.header = scan_1->header;

  merged.angle_min = std::min(scan_1->angle_min, scan_2->angle_min + angle_modif);
  merged.angle_max = std::max(scan_1->angle_max, scan_2->angle_max + angle_modif);

  merged.angle_increment = smaller->angle_increment;

  merged.range_min = std::min(scan_1->range_min, scan_2->range_min);
  merged.range_max = std::max(scan_1->range_max, scan_2->range_max);

  merged.time_increment =
      std::max(scan_1->time_increment, scan_2->time_increment);
  merged.scan_time = std::max(scan_1->scan_time, scan_2->scan_time);

  // Calculate number of bins and pre-allocate vector to hold appropriate number
  // of elements
  int bins = ceil((std::fabs(merged.angle_min) + std::fabs(merged.angle_max)) /
                  merged.angle_increment);
  int offset = 0;

  std::vector<float> ranges;
  ranges.resize(bins, -1);

  // Copy the smaller scan ranges into merged scan ranges from angled offset
  offset = std::fabs((merged.angle_min - smaller->angle_min) /
                     merged.angle_increment);
  ranges.insert(ranges.begin() + offset, smaller->ranges.begin(),
                smaller->ranges.end());

  // Copy larger scan ranges into merged scan accounting for changing incrememnt
  offset = std::fabs((merged.angle_min - larger->angle_min+angle_modif) /
                     merged.angle_increment);
  int slices_per_bin = ceil(larger->angle_increment / merged.angle_increment);

  float inf = std::numeric_limits<double>::infinity();

  for (int i = 0; i < larger->ranges.size(); i++) {
    int index =
        offset + i * slices_per_bin + slices_per_bin / 2 - slices_per_bin / 2;

    double range = larger->ranges[i];

    if (range < larger->range_min) continue;

    if (range >= ranges[index] && ranges[index] != -1) continue;

    ranges[index] = (range <= larger->range_max) ? range : inf;
  }

  merged.ranges = ranges;
  pub.publish(merged);
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "laser_merge");

  ros::NodeHandle nh;

  pub = nh.advertise<LaserScan>("scan/merged", 50);

  message_filters::Subscriber<LaserScan> scan_1_sub(nh, "scan_1", 1);
  message_filters::Subscriber<LaserScan> scan_2_sub(nh, "scan_2", 1);

  typedef sync_policies::ApproximateTime<LaserScan, LaserScan> SyncPolicy;
  // ApproximateTime takes a queue size as its constructor argument, hence
  // MySyncPolicy(10)
  Synchronizer<SyncPolicy> sync(SyncPolicy(10), scan_1_sub, scan_2_sub);
  sync.registerCallback(boost::bind(&callback, _1, _2));

  ros::spin();

  return 0;
}
