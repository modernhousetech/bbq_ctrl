var alarmed = flow.get(msg.topic) || false;
if (!alarmed) {
    flow.set(msg.topic, true);
    hass = global.get("homeassistant");
    var info = hass.homeAssistant.states[msg.topic];
    msg.payload = "Alert: " + info.attributes.friendly_name + " (" + 
      info.state + info.attributes.unit_of_measurement + ") is high!";
    // Send to all audio players
    msg.speak_to = "";
    // Send to all phones
    msg.notify_to = "";
    return msg;
}

var alarmed = flow.get(msg.topic) || false;
if (alarmed) {
    flow.set(msg.topic, false);
    // Send to all audio players
    msg.speak_to = "";
    // Send to all phones
    msg.notify_to = "";
    msg.payload = "freezer temperature OK";
    return msg;
}