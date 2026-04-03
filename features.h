#ifndef FEATURES_H
#define FEATURES_H

#include "../feature.h"
#include <memory>

// Factory functions for each feature
std::unique_ptr<Feature> create_esp_feature();
std::unique_ptr<Feature> create_aimbot_feature();
std::unique_ptr<Feature> create_radar_feature();
std::unique_ptr<Feature> create_triggerbot_feature();
std::unique_ptr<Feature> create_misc_feature();

#endif // FEATURES_H
