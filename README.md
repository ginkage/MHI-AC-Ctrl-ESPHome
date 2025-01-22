[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/ginkage)
[![paypal RobertJansen1](https://www.paypalobjects.com/en_GB/i/btn/btn_donate_LG.gif)](https://www.paypal.com/donate/?hosted_button_id=TL3SFZ4P6ZDHN)

# MHI-AC-Ctrl-ESPHome
This project is a simple integration of the amazing work [absalom-muc](https://github.com/absalom-muc) has done with his project [MHI-AC-Ctrl](https://github.com/absalom-muc/MHI-AC-Ctrl).\
It's supposed to simplify the [Home Assistant](https://www.home-assistant.io/) setup, while giving you OTA and auto-discovery with virtually zero effort and no MQTT needed, powered by [ESPHome](https://esphome.io/).\
MHI-AC-Ctrl-core.\* files were forked directly, with no modification, whereas your WiFi credentials should go into the \*.yaml file, and mhi_ac_ctrl.h is the core of the integration.\
Create a new device within ESPHome and combine the yaml with example.yaml, rename example_ac and example-ac and install!

# Fan Modes Up/Down Left/Right
Most newer MHI units (the ones supporting the WF-RAC WiFi module) support fine grained vane control for Left/Right and Up/Down.  
When your log is flooded with mhi_ac_ctrl_core.loop error: -2 errors after updating to the newer code, please change your yaml file to include the legacy file instead of the large_framesize.yaml.  
Currently the MHI code allows for more fine grained fan direction than esphome climate supports. for that, additional template parts are added.  
There are 8 modes for Left/Right: Left, Left/Center, Center, Center/Right, Right, Wide, Spot and Swing  
There are 5 modes for Up/Down: Up, Up/Center, Center/Down, Down and Swing  
Setting swing from the esphome climate now fully works. It will store the oldvanes mode, and configure swing. after disabling swing (either vertically, horizontally or off), the old settings will be restored. Manually changing modes for Left/Right or Up/Down will update the climate state as well.

> [!TIP]
> Hardware designed by fonske (can be ordered at JLCPCB) can be found [here](JLCPCB/Hardware.md) Include Gerber file, bom and pick and place files.


# Climate Quiet

Climate Quiet was added to ESPhome, so QUIET was added. ordering still needs work (https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/22#issuecomment-1744448983)
Added the solution for the auto mode from: https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/22#issuecomment-1310271934:
CLIMATE_FAN_DIFFUSE in fan speed and status sections and reshuffle the numbers and add CLIMATE_FAN_DIFFUSE to the traits.set_supported_fan_modes

Has now 5 different fan modes but I'm not sure if the auto mode works proper, keep testing.

# Changelog:

**v3.0** (2024-08)
 - Breaking change: moved all files to component and allow for easy install, thanks to @XMaarten and https://github.com/hberntsen/mhi-ac-ctrl-esp32-c3
   - When you are upgrading from v2.1 or older, and experience compile errors, please see https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/100#issuecomment-2395388853 for manual cleanup steps
 - Manually downloading the files to your Home Assistant setup is no longer needed
 - Legacy or Large framesize files are merged again

**v2.1** (2024-03)
 - Breaking change: Cleaned up conf files
 - Add restart button
 - Update Home Assistant naming convention
 - Enable energy dashboard usage 

**v2.0** (2024-01)
 - Based on absalom-muc v2.8 (September 2023)
 - Breaking change in YAML configuration (need to set frame_size in globals)
 - Added legacy support configurable from YAML (removing 3d auto and vanes LR control)

# FAQ

### I have no clue where to start, what can i do?
Step 1: learn dutch to understand the blogpost (or use some translation service :) 
Step 2: go to this extensive blogpost: https://www.twoenter.nl/blog/smarthome/mitsubishi-airco-voorzien-van-wifi-besturing/ 
  
### I am getting the following logline in the console of my device:   
[W][component:237]: Component MhiAcCtrl took a long time for an operation (52 ms).  
[W][component:238]: Components should block for at most 30 ms.  

This is because esphome now alerts for slow components, to ignore this warning you can configure logging for components to ERROR, this will suppress the WARNING
```yaml
# Enable logging
logger:
    level: INFO
    baud_rate: 0
    logs:
        component: ERROR
```

See https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/61 for more information  

### I am getting mhi_ac_ctrl_core.loop error: -2 errors and nothing works!  
You probably have an older version of the Airco unit which doesn't support the newer, larger framesize.   
You can to change the frame_size in the yaml to 20 to disable newer functionality.  
when framsize is set to 20, 3D auto, and vane Left / Right doesn't work, vane Up / Down works limited.  
All other features should work fine.  


# License
This project is licensed under the MIT License - see the LICENSE file for details.\
(TL;DR: Do whatever you want with the code, no warranty given, give credit where it's due.)
