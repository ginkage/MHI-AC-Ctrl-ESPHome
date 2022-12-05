# Adding a sensor for the HomeAssistant individual devices energy Dashboard

The airco .yaml file needs to be extended with the following:

      - name: ${devicename} current power
        id: "current"

  - platform: template
    name: ${devicename} power
    id: "power"
    lambda: return id(current).state * 230;  #I think all units are 230V; real voltage can vary a bit, but has a small impact.
    unit_of_measurement: W
    device_class: energy
  - platform: total_daily_energy
    name: ${devicename} daily energy
    power_id: "power"
    unit_of_measurement: Wh
    device_class: energy



# Home Assistant
In the HA energy dashboard, you can track/show individual devices. However, each device needs this additional sensor.
With this sensor in place, you can add this via:

- **Settings**
- **Dashboards**
- **Energy**
- **Individual Devices**
add $devicename daily energy

From here on, HA shows the airco unit next to other devices, and shows a 'top speaker' like graph.
It takes an hour before the graph is drawn in the energy dashboard.
