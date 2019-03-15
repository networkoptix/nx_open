/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   openalprimpl.cpp
 * Author: mhill
 * 
 * Created on February 24, 2018, 10:01 AM
 */

#include "openalprimpl.h"
#include "dsopenalpr_lib.h"

#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <ctime>
const int GPU_ID = 0;


OpenAlprImpl::OpenAlprImpl(const char* country, const char* config_file, 
        const char* runtime_dir, const char* license_key ) :
  m_alpr(country, config_file, runtime_dir, license_key), 
  m_alprstream(MAX_FRAME_QUEUE_SIZE, false)
{
  m_vehicleclassifier = NULL;
  m_alprstream.set_parked_vehicle_detect_params(false, 0,0,0);
  
  this->config_file = config_file;
  this->runtime_dir = runtime_dir;
  this->license_key = license_key;
  this->country = country;
  
  this->grouping_enabled = true;
}


OpenAlprImpl::~OpenAlprImpl() {
  if (m_vehicleclassifier != NULL)
    delete m_vehicleclassifier;
}

void OpenAlprImpl::push_cv_mat(unsigned char* data, int frame_width, int frame_height, int frame_channels) {
  m_alprstream.push_frame(data, frame_channels, frame_width, frame_height);
}

void OpenAlprImpl::enable_vehicle_classifier() {
  if (m_vehicleclassifier == NULL)
  {
    // vehicle classifier currently supports batch size 1
    m_vehicleclassifier = new alpr::VehicleClassifier(config_file, runtime_dir, alpr::VEHICLEDETECTOR_GPU, 1, GPU_ID, license_key);
  }
}

void OpenAlprImpl::disable_grouping() {
  // Set the params such that groups are instantly expired 
  this->m_alprstream.set_group_parameters(1, 1, 100, 0);
  this->grouping_enabled = false;
}

std::vector<alpr::AlprResults> OpenAlprImpl::process_batch() {
//  std::cout << "Processing batch" << std::endl;
  const clock_t begin_time = clock();
  std::vector<alpr::RecognizedFrame> frame_results = m_alprstream.process_batch(&m_alpr);
//  std::cout << "OpenALPR Batch processing time: " << float( clock () - begin_time ) /  CLOCKS_PER_SEC << " sec" << std::endl ;
  
  std::vector<alpr::AlprResults> results;
  for (int i = 0; i < frame_results.size(); i++)
  {
    results.push_back(frame_results[i].results);
  }
  return results;
}

std::vector<alpr::AlprGroupResult> OpenAlprImpl::pop_groups() {
  std::vector<alpr::AlprGroupResult> group_results = m_alprstream.pop_completed_groups();
  
  // If grouping is not enabled, just pop the groups (to clean up memory) and return empty
  if (!grouping_enabled)
  {
    std::vector<alpr::AlprGroupResult> empty_group_results;
    return empty_group_results;
  }
  
  // If vehicle classification is enabled, run once for each group
  if (group_results.size() > 0 && m_vehicleclassifier != NULL)
  {
    const clock_t begin_time = clock();
    for (int i = 0; i < group_results.size(); i++)
      m_alprstream.recognize_vehicle(&group_results[i], m_vehicleclassifier);
    //std::cout << "OpenALPR vehicle classification time (" << group_results.size() << " vehicles): " << float( clock () - begin_time ) /  CLOCKS_PER_SEC << " sec" << std::endl;
    
  }
  
  return group_results;
  
}



