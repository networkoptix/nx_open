/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   openalprimpl.h
 * Author: mhill
 *
 * Created on February 24, 2018, 10:01 AM
 */

#ifndef OPENALPRIMPL_H
#define OPENALPRIMPL_H

#include <alpr.h>
#include <alprstream.h>
#include <vehicle_classifier.h>

class OpenAlprImpl {
public:
  OpenAlprImpl(const char* country, const char* config_file, const char* runtime_dir, const char* license_key);
  virtual ~OpenAlprImpl();
  
  void push_cv_mat(unsigned char *data, int frame_width, int frame_height, int frame_channels);
  
  void enable_vehicle_classifier();
  void disable_grouping();
  
  std::vector<alpr::AlprResults> process_batch();
  
  std::vector<alpr::AlprGroupResult> pop_groups();
private:

  std::string config_file;
  std::string runtime_dir;
  std::string license_key;
  std::string country;
  
  bool grouping_enabled;
  
  alpr::Alpr m_alpr;
  alpr::AlprStream m_alprstream;
  
  alpr::VehicleClassifier* m_vehicleclassifier;
};

#endif /* OPENALPRIMPL_H */

