# MHI-AC-Ctrl-ESPHome
This project is a simple integration of the amazing work [absalom-muc](https://github.com/absalom-muc) has done with his project [MHI-AC-Ctrl](https://github.com/absalom-muc/MHI-AC-Ctrl).\
It's supposed to simplify the [Home Assistant](https://www.home-assistant.io/) setup, while giving you OTA and auto-discovery with virtually zero effort and no MQTT needed, powered by [ESPHome](https://esphome.io/).\
MHI-AC-Ctrl-core.\* files were forked directly, with no modification, whereas your WiFi credentials should go into the \*.yaml file, and mhi_ac_ctrl.h is the core of the integration.\
Just put all these files in your ESPHome folder, flash once, and you're good to go!

# Fan Auto Mode with 4 fan speeds
Added the solution for the auto mode from: https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/22#issuecomment-1310271934
Now gets 5 different fan modes but I'm not sure if the auto mode works, keep testing.

# License
This project is licensed under the MIT License - see the LICENSE file for details.\
(TL;DR: Do whatever you want with the code, no warranty given, give credit where it's due.)
