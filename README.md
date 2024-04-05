[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/ginkage)
[![paypal RobertJansen1](https://www.paypalobjects.com/en_GB/i/btn/btn_donate_LG.gif)](https://www.paypal.com/donate/?hosted_button_id=TL3SFZ4P6ZDHN)

# MHI-AC-Ctrl-ESPHome
This project is a simple integration of the amazing work [absalom-muc](https://github.com/absalom-muc) has done with his project [MHI-AC-Ctrl](https://github.com/absalom-muc/MHI-AC-Ctrl).\
It's supposed to simplify the [Home Assistant](https://www.home-assistant.io/) setup, while giving you OTA and auto-discovery with virtually zero effort and no MQTT needed, powered by [ESPHome](https://esphome.io/).\
MHI-AC-Ctrl-core.\* files were forked directly, with no modification, whereas your WiFi credentials should go into the \*.yaml file, and mhi_ac_ctrl.h is the core of the integration.

# Installation
## Prerequisites:
 - Home Assistant installed  
 - ESPHome plugin installed (https://esphome.io/guides/getting_started_hassio.html)  
 - ssh plugin (preferred) or file editor installed in Home Assistant  

SSH to your Home Assistant system, navigate to /config/esphome and clone the git repo:
```bash
cd /config/esphome
git clone https://github.com/ginkage/MHI-AC-Ctrl-ESPHome.git .
cp lr_mhi_ac_ctrl.yaml first_ac.yaml
```

After that, you should have a first_ac.yaml. Verify the file (wifi credentials, and name of the unit for example) Login to the Home Assistant web interface and install the yaml file on your esp with esphome.


# Fan Modes Up/Down Left/Right
Most newer MHI units (the ones supporting the WF-RAC WiFi module) support fine grained vane control for Left/Right and Up/Down.  
When your log is flooded with mhi_ac_ctrl_core.loop error: -2 errors after updating to the newer code, please change your yaml file to include the legacy file instead of the large_framesize.yaml.  
Currently the MHI code allows for more fine grained fan direction than esphome climate supports. for that, additional template parts are added.  
There are 8 modes for Left/Right: Left, Left/Center, Center, Center/Right, Right, Wide, Spot and Swing  
There are 5 modes for Up/Down: Up, Up/Center, Center/Down, Down and Swing  
Setting swing from the esphome climate now fully works. It will store the oldvanes mode, and configure swing. after disabling swing (either vertically, horizontally or off), the old settings will be restored. Manually changing modes for Left/Right or Up/Down will update the climate state as well.

# Climate Quiet

Climate Quiet was added to ESPhome, so QUIET was added. ordering still needs work (https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/22#issuecomment-1744448983)
Added the solution for the auto mode from: https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/22#issuecomment-1310271934:
CLIMATE_FAN_DIFFUSE in fan speed and status sections and reshuffle the numbers and add CLIMATE_FAN_DIFFUSE to the traits.set_supported_fan_modes

Has now 5 different fan modes but I'm not sure if the auto mode works proper, keep testing.

# Changelog:

**v2.2** (2024-04)
 - Add low temp heating from @Terminator-NL
 - Add temp offset from @Terminator-NL
 - Add webserver with authentication
 - Made wifi credentials more nice
 - Add defrost binary sensor
 - Add high precision temperature setting

**v2.1** (2024-03)
 - Breaking change: Cleaned up conf files
 - Add restart button
 - Update Home Assistant naming convention
 - Enable energy dashboard usage 

**v2.0** (2024-01)
 - Based on absalom-muc v2.8 (September 2023)
 - Breaking change in YAML configuration (need to set frame_size in globals)
 - Added legacy support configurable from YAML (removing 3d auto and vanes LR control)

# License
This project is licensed under the MIT License - see the LICENSE file for details.\
(TL;DR: Do whatever you want with the code, no warranty given, give credit where it's due.)
