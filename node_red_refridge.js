var hass = global.get("homeassistant");

try {
    
    // Prerequisites -- assemble values, etc.

    if (hass.homeAssistant.states["input_boolean.refridge_alerts_enabled"].state !== 'on') {
        return null;
    }

    var max_temp;
    var min_temp;
    var realarm_increment = 5;
    
    if (msg.topic.includes("freezer")) {
        max_temp = parseFloat(hass.homeAssistant.states["input_number.refridge_freezer_high"].state) || 5;
        min_temp = parseFloat(hass.homeAssistant.states["input_number.refridge_freezer_low"].state) || -40;
    } else {
        max_temp = parseFloat(hass.homeAssistant.states["input_number.refridge_fridge_high"].state) || 40;
        min_temp = parseFloat(hass.homeAssistant.states["input_number.refridge_fridge_low"].state) || 33;
    }

    // Get specific min/max for device if it exists
    var temp;
    temp = hass.homeAssistant.states["input_number." + msg.topic.split(".")[1] + "_high"].state;
    if (temp && !isNaN(temp = parseFloat(temp))) {
        max_temp = temp;
    }
    temp = hass.homeAssistant.states["input_number." + msg.topic.split(".")[1] + "_low"].state;
    if (temp && !isNaN(temp = parseFloat(temp))) {
        min_temp = temp;
    }
    
    var state = msg.payload;
    msg.payload = null;

    var not_a_number;
    var cur_temp;
    
    if (isNaN(state)) {
        // Is the probe not reporting? Battery probably dead
        not_a_number = true;
    } else {
        not_a_number = false;
        cur_temp = parseFloat(state);
    }

    // if not a number we set off_by_cur to 100
    var off_by_cur = not_a_number ? 100.0 : max_temp - cur_temp;

    // We want to notify user when:
    //  cur temp is higher that max AND  (we have
    var alarmedHigh = flow.get(msg.topic + "_h");
    if (alarmedHigh) {
        off_by_last_check = alarmedHigh;
    } else {
        off_by_last_check = 0;
    }

    var info;

    if (not_a_number || off_by_cur > 0) {
        // Temp is too high!

        // Only want one extant alarm at a time. If we have a too low
        // then delete it.
        flow.set(msg.topic + "_l", undefined);

        // off_cur = 15, off_before = 2 === 12
        // Already alarmed too high? 
        if (!alarmedHigh || (off_by_cur - off_by_last_check) > realarm_increment ) {
            // No, then notify
            flow.set(msg.topic + "_h", off_by_cur);

            info = hass.homeAssistant.states[msg.topic];
            msg.payload = "Alert: " + info.attributes.friendly_name + "\\n";
            
            if (not_a_number) {
                msg.payload += " is reporting '" + info.state + "'. Is the the battery dead?";

            } else {
                msg.payload += " (" + 
                info.state + info.attributes.unit_of_measurement + ") is high!";
            }
        }
    } else {
        // Not too high.
        // Is it too low?

        var alarmedLow;

        // Note: we know that the current temp is a number by now
        // min = 32, cur = 30 ==== 2
        off_by_cur = cur_temp - min_temp;
        if (off_by_cur > 0) {
            // Cur temp too low

            // Only want one extant alarm at a time. If we have a too high
            // then delete it.
            flow.set(msg.topic + "_h", undefined);

            // Have we alarmed about this before?
            alarmedLow = flow.get(msg.topic + "_l");
            if (!alarmedLow || off_by_cur - alarmedLow > realarm_increment) {
                // Either have not alarmed or need to re-alarm

                flow.set(msg.topic + "_l", off_by_cur);

                info = hass.homeAssistant.states[msg.topic];
                msg.payload = "Alert: " + info.attributes.friendly_name + "\\n";
                
                msg.payload += " (" + 
                info.state + info.attributes.unit_of_measurement + ") is low!";
            }
        } else {
            // Goldilocks. Not to high, not to low

            flow.set(msg.topic + "_h", undefined);
            flow.set(msg.topic + "_l", undefined);

            // Last thing. Do we have an alarm extant? If so we remove 
            // alarm and notify user that all is now ok.
       
        
            // Now, did we alarm earlier and now want to tell user it's now ok?
            //  or, is the cur_temp now too low
            if (alarmedHigh || alarmedLow) {

                info = hass.homeAssistant.states[msg.topic];

                msg.payload = info.attributes.friendly_name + " (" + 
                info.state + info.attributes.unit_of_measurement + ") now OK";
                              
            }
        }        
    }
    
    if (!off_by_cur && cur_temp < min_temp) { 
        // We are not too high (takes precedence) but are we too low 
        
    }
    
    msg.item = hass.homeAssistant.states[msg.topic];

    if (msg.payload) {

        msg.src = "refridge";
        // Send to all audio players
        msg.speak_to = "";
        // Send to all phones
        msg.notify_to = "";

        return msg;
    }

} catch (error) {
    //hass.homeAssistant.states["input_number.battery_alerts_failures"].state
    //msg.payload = "Error running battery level management";
    return { src: "refridge", payload: "Error running refridgerator temperature monitor"};
}


var hass = global.get("homeassistant");

// Debugging vars:
// Always send alert, ignore "already sent" logic
var dbgAlwaysSend = true;

//try {
    
    // If we have a specific high/low for this device then get and return
    function getSpecificLimit(entityName, general) {
        var specificTemp;
        var specificEntitiy;
        specificEntitiy = hass.homeAssistant.states["input_number." + entityName];
        if (specificEntitiy) {
            specificTemp = specificEntitiy.state;
        }
        if (specificTemp && !isNaN(specificTemp = parseFloat(specificTemp))) {
            return specificTemp;
        }
        
        return general;
    }
    
    // Prerequisites -- assemble values, etc.

    if (hass.homeAssistant.states["input_boolean.refridge_alerts_enabled"].state !== 'on') {
        return null;
    }

    var max_temp;
    var min_temp;
    var realarm_increment = 5;
    
    if (msg.topic.includes("freezer")) {
        max_temp = parseFloat(hass.homeAssistant.states["input_number.refridge_freezer_high"].state) || 5;
        min_temp = parseFloat(hass.homeAssistant.states["input_number.refridge_freezer_low"].state) || -40;
    } else {
        max_temp = parseFloat(hass.homeAssistant.states["input_number.refridge_fridge_high"].state) || 40;
        min_temp = parseFloat(hass.homeAssistant.states["input_number.refridge_fridge_low"].state) || 33;
    }

    var entitityName = msg.topic.split(".")[1];
    max_temp = getSpecificLimit(entitityName + "_high", max_temp);
    min_temp = getSpecificLimit(entitityName + "_low", min_temp);

    var state = msg.payload;
    msg.payload = null;

    var not_a_number;
    var cur_temp;

    if (isNaN(state)) {
        // Is the probe not reporting? Battery probably dead
        not_a_number = true;
    } else {
        not_a_number = false;
        cur_temp = parseFloat(state);
    }

    // if not a number we set off_by_cur to 100
    var off_by_cur = (not_a_number ? 100.0 : max_temp) - cur_temp;

    // We want to notify user when:
    //  cur temp is higher that max AND  (we have
    var alarmedHigh = dbgAlwaysSend ? undefined : flow.get(msg.topic + "_h");
    if (alarmedHigh) {
        off_by_last_check = alarmedHigh;
    } else {
        off_by_last_check = 0;
    }

    var info;

    if (not_a_number || off_by_cur > 0) {
        // Temp is too high!

        // Only want one extant alarm at a time. If we have a too low
        // then delete it.
        flow.set(msg.topic + "_l", undefined);

        // off_cur = 15, off_before = 2 === 12
        // Already alarmed too high? 
        if (!alarmedHigh || (off_by_cur - off_by_last_check) > realarm_increment ) {
            // No, then notify
            flow.set(msg.topic + "_h", off_by_cur);

            info = hass.homeAssistant.states[msg.topic];
            msg.payload = "Alert: " + info.attributes.friendly_name + "\\n";
            
            if (not_a_number) {
                msg.payload += " is reporting '" + info.state + "'. Is the the battery dead?";

            } else {
                msg.payload += " (" + 
                info.state + info.attributes.unit_of_measurement + ") is high!";
            }
        }
    } else {
        // Not too high.
        // Is it too low?

        var alarmedLow;

        // Note: we know that the current temp is a number by now
        // min = 32, cur = 30 ==== 2
        off_by_cur = cur_temp - min_temp;
        if (off_by_cur > 0) {
            // Cur temp too low

            // Only want one extant alarm at a time. If we have a too high
            // then delete it.
            flow.set(msg.topic + "_h", undefined);

            // Have we alarmed about this before?
            alarmedLow = dbgAlwaysSend ? undefined : flow.get(msg.topic + "_l");
            if (!alarmedLow || off_by_cur - alarmedLow > realarm_increment) {
                // Either have not alarmed or need to re-alarm

                flow.set(msg.topic + "_l", off_by_cur);

                info = hass.homeAssistant.states[msg.topic];
                msg.payload = "Alert: " + info.attributes.friendly_name + "\\n";
                
                msg.payload += " (" + 
                info.state + info.attributes.unit_of_measurement + ") is low!";
            }
        } else {
            // Goldilocks. Not to high, not to low

            flow.set(msg.topic + "_h", undefined);
            flow.set(msg.topic + "_l", undefined);

            // Last thing. Do we have an alarm extant? If so we remove 
            // alarm and notify user that all is now ok.
       
        
            // Now, did we alarm earlier and now want to tell user it's now ok?
            //  or, is the cur_temp now too low
            if (alarmedHigh || alarmedLow) {

                info = hass.homeAssistant.states[msg.topic];

                msg.payload = info.attributes.friendly_name + " (" + 
                info.state + info.attributes.unit_of_measurement + ") now OK";
                              
            }
        }        
    }
    
    if (!off_by_cur && cur_temp < min_temp) { 
        // We are not too high (takes precedence) but are we too low 
        
    }
    
    msg.item = hass.homeAssistant.states[msg.topic];

    if (msg.payload) {

        msg.src = "refridge";
        // Send to all audio players
        msg.speak_to = "";
        // Send to all phones
        msg.notify_to = "";

        return [msg, msg];
    }
        return [null, msg];

// } catch (e) {
//     var eMsg = { payload: "Error running refridgerator temperature monitor: " + e.message, 
//         src: "refridge", level: 100, notify_to: ""};
//     msg.eMsg = eMsg;
//     return [eMsg, msg];
// }

var msgTs = new Date().getTime();
var ms_diff = msgTs - (flow.get(msg.entity_id) || 0);
