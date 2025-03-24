#include "mhi_binary_sensors.h"

namespace esphome {
namespace mhi {

static const char* TAG = "mhi.binary_sensor";

void MhiBinarySensors::set_defrost(BinarySensor* sensor){ defrost_ = sensor; }
void MhiBinarySensors::set_vanes_3d_auto_enabled(BinarySensor* sensor){ vanes_3d_auto_enabled_ = sensor; }



void MhiBinarySensors::setup() {
    this->parent_->add_listener(this);
}

void MhiBinarySensors::dump_config() {

    //LOG_CLIMATE(TAG, "MHI-AC-Ctrl Binary Sensors", this);
}

void MhiBinarySensors::update_status(ACStatus status, int value) {

    switch (status) {
    case opdata_defrost:
        // if (value)
        //     output_P(status, PSTR(TOPIC_DEFROST), PSTR(PAYLOAD_OP_DEFROST_ON));
        // else
        //     output_P(status, PSTR(TOPIC_DEFROST), PSTR(PAYLOAD_OP_DEFROST_OFF));
        if (this->defrost_ != NULL) { 
            this->defrost_ -> publish_state(value != 0); 
        }
        break;
    case status_3Dauto:
        if (this->vanes_3d_auto_enabled_ != NULL) { 
            switch (value) {
            case 0b00000000:
                this->vanes_3d_auto_enabled_ -> publish_state(false); 
                break;
            case 0b00000100:
                this->vanes_3d_auto_enabled_ -> publish_state(true); 
                break;
            }
        }
        break;
    default:
        // skip these values as they are not used currently
        break;
    }
}

} //namespace mhi
} //namespace esphome