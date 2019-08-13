hass = global.get("homeassistant");

try {
    
    if (hass.homeAssistant.states["input_boolean.refridge_alerts_disable"].state == 'on') {
        return null;
    }

    var alertLevel;
    
    if (msg.topic.includes("freezer")) {
        alertLevel = parseInt(hass.homeAssistant.states["input_number.refridge_freezer_level"].state) || 0;
        msg.freezer = alertLevel;
    } else {
        alertLevel = parseInt(hass.homeAssistant.states["input_number.refridge_fridge_level"].state) || 40;
        msg.fridge = alertLevel;
    }

    var state = msg.payload;
    msg.payload = null;

    var not_a_number;
    var temp;

    if (isNaN(state)) {
        // Is the probe not reporting? Battery probably dead
        not_a_number = true;
    } else {
        not_a_number = false;
        temp = parseFloat(state);
    }

    var alarmed = flow.get(msg.topic) || false;
    if (not_a_number || temp > alertLevel) {
        if (!alarmed) {
            flow.set(msg.topic, true);

            var info = hass.homeAssistant.states[msg.topic];
            msg.payload = "Alert: " + info.attributes.friendly_name + "\\n";
            
            if (not_a_number) {
                msg.payload += " is reporting \"" + info.state + "\". Is the the battery dead?";

            } else {
                msg.payload += " (" + 
                info.state + info.attributes.unit_of_measurement + ") is high!";
            }
        }
    } else {
        if (alarmed) {
            flow.set(msg.topic, false);

            var info = hass.homeAssistant.states[msg.topic];

            msg.payload = info.attributes.friendly_name + " (" + 
            info.state + info.attributes.unit_of_measurement + ") now OK";
        }        
    }
    msg.item = hass.homeAssistant.states[msg.topic];

    if (msg.payload) {

        // Send to all audio players
        msg.speak_to = "";
        // Send to all phones
        msg.notify_to = "";

        return msg;
    }

} catch (error) {
    //hass.homeAssistant.states["input_number.battery_alerts_failures"].state
    //msg.payload = "Error running battery level management";
    return { payload: "Error running refridgerator temperature monitor"};
}
