[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/ginkage)
[![paypal](https://www.paypalobjects.com/en_GB/i/btn/btn_donate_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=LF9S5WAF6E4VA)

# MHI-AC-Ctrl-ESPHome
This project is a simple integration of the amazing work [absalom-muc](https://github.com/absalom-muc) has done with his project [MHI-AC-Ctrl](https://github.com/absalom-muc/MHI-AC-Ctrl).\
It's supposed to simplify the [Home Assistant](https://www.home-assistant.io/) setup, while giving you OTA and auto-discovery with virtually zero effort and no MQTT needed, powered by [ESPHome](https://esphome.io/).\
MHI-AC-Ctrl-core.\* files were forked directly, with no modification, whereas your WiFi credentials should go into the \*.yaml file, and mhi_ac_ctrl.h is the core of the integration.\
Just put all these files in your ESPHome folder, flash once, and you're good to go!

# Fan Modes Up/Down Left/Right
Currently the MHI code allows for more fine grained fan direction than esphome climate supports. for that, additional template parts are added.
There are 7 modes for Left/Right: Left, Left/Center, Center, Center/Right, Right, Wide, Spot and Swing
There are 5 modes for Up/Down: Up, Up/Center, Center/Down, Down and Swing
Setting swing from the esphome climate now fully works. It will store the oldvanes mode, and configure swing. after disabling swing (either vertically, horizontally or off), the old settings will be restored. Manually changing modes for Left/Right or Up/Down will update the climate state as well.

# Climate Quiet

Climate Quiet was added to ESPhome, so QUIET was added. ordering still needs work (https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/22#issuecomment-1744448983)
Added the solution for the auto mode from: https://github.com/ginkage/MHI-AC-Ctrl-ESPHome/issues/22#issuecomment-1310271934:
CLIMATE_FAN_DIFFUSE in fan speed and status sections and reshuffle the numbers and add CLIMATE_FAN_DIFFUSE to the traits.set_supported_fan_modes

Has now 5 different fan modes but I'm not sure if the auto mode works proper, keep testing.

# License
This project is licensed under the MIT License - see the LICENSE file for details.\
(TL;DR: Do whatever you want with the code, no warranty given, give credit where it's due.)
