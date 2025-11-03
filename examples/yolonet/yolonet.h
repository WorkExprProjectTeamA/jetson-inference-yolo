#ifndef __MAIN_H__
#define __MAIN_H__

#include <mutex>
#include <atomic>

#include "videoSource.h"
#include "videoOutput.h"

#include "yoloNet.h"
#include "objectTracker.h"
#include "gstSpeaker.h"
#include "customNetwork.h"


static std::unordered_map<int, int> alertClassPriorities = {
    // 최고위험 (4) - 화재/폭발/즉시위험
    {26, 4}, // combustible_flammable_materials
    {39, 4}, // combustible_intrusion_welding_zone 
    {40, 4}, // smoking_in_non_smoking_area 
    {25, 4}, // foreign_material_water_oil
    {33, 4}, // cargo_collapse_during_transport
	{7, 4},  // smoking 
    // 고위험 (3) - 인명사고 위험
    {34, 3}, // person_in_forklift_pathway
    {45, 3}, // person_behind_reversing_vehicle
    {1, 3},  // person_without_workwear
    {32, 3}, // unstable_individual_cargo_loading
    {36, 3}, // poor_loading_condition_collapse
	{55, 1}, // forklift_driving_outside_designated_area
    // 중위험 (2) - 구조적 위험/안전규정 위반
    {30, 2}, // stacking_three_levels_or_more 
	{31, 2}, // poor_rack_storage_condition
    {35, 2}, // safety_rule_violation
    {41, 2}, // person_in_cargo_compartment_loading
    {42, 2}, // person_in_cargo_compartment_unloading 
    // 저위험 (1) - 예방/관리 차원
    {24, 1}, // welding_equipment 
    {27, 1}, // sandwich_panel 
    {28, 1}, // forklift_transport_limited_visibility 
    {29, 1}, // rack_loading_surrounding_obstacles 
    {37, 1}, // person_in_external_work_zone 
    {38, 1}, // hand_pallet_cart_two_level_stacking 
    {43, 1}, // insufficient_forklift_path_marking
    {44, 1}, // obstacle_in_front_dock_door 
    {46, 1}, // disorganized_empty_pallets
    {47, 1}, // leaning_inside_rack_safety_line
    {48, 1}, // pallet_warping_damage_corrosion 
    {49, 1}, // person_riding_freight_elevator 
    {50, 1}, // power_strip_without_overload_breaker
    {51, 1}, // missing_fire_extinguisher 
    {52, 1}, // restricted_area_door_open 
    {53, 1}, // cargo_in_escape_route
    {54, 1}  // dock_unloading_disconnected 
};

// class ImageBuffer {
// private:
//     static ImageBuffer* instance;
//     static std::mutex instanceMutex;

//     mutable std::mutex dataMutex;
//     uchar3* image;
//     int width;
//     int height;

//     ImageBuffer() : image(nullptr), width(0), height(0) {}

// public:
//     static ImageBuffer* getInstance() {
//         std::lock_guard<std::mutex> lock(instanceMutex);
//         if (instance == nullptr) {
//             instance = new ImageBuffer();
//         }
//         return instance;
//     }

//     void setImage(uchar3* img, int w, int h) {
//         std::lock_guard<std::mutex> lock(dataMutex);
//         image = img;
//         width = w;
//         height = h;
//     }

//     bool getImage(uchar3*& img, int& w, int& h) {
//         std::unique_lock<std::mutex> lock(dataMutex);

//         img = image;
//         w = width;
//         h = height;

//         return img != nullptr;
//     }
// };

// class DetectionResults {
// private:
//     static DetectionResults* instance;
//     static std::mutex instanceMutex;

//     mutable std::mutex dataMutex;
//     yoloNet::Detection* detections;
//     int numDetections;
//     uchar3* image;
//     int width;
//     int height;

//     DetectionResults() : detections(nullptr), numDetections(0), image(nullptr), width(0), height(0) {}

// public:
//     static DetectionResults* getInstance() {
//         std::lock_guard<std::mutex> lock(instanceMutex);
//         if (instance == nullptr) {
//             instance = new DetectionResults();
//         }
//         return instance;
//     }

//     void setResults(yoloNet::Detection* dets, int numDets, uchar3* img, int w, int h) {
//         std::lock_guard<std::mutex> lock(dataMutex);
//         detections = dets;
//         numDetections = numDets;
//         image = img;
//         width = w;
//         height = h;
//     }

//     bool getResults(yoloNet::Detection*& dets, int& numDets, uchar3*& img, int& w, int& h) {
//         std::unique_lock<std::mutex> lock(dataMutex);

//         dets = detections;
//         numDets = numDetections;
//         img = image;
//         w = width;
//         h = height;

//         return dets != nullptr;
//     }
// };

// void captureImages(videoSource* input);

// void processInference(yoloNet* net, uint32_t overlayFlags);

// void renderOutput(videoOutput* output, yoloNet* net);



#endif