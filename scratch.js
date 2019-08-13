

    hass = global.get("homeassistant");
    
    var needToNotify = false;

    function getDb(name) {
        var _db = {
            data: [],
            getItem: function () {}
        }
        // Get our battery level "database" as text, then convert to array
        var db_text = hass.homeAssistant.states["input_text." + name + "_db"].state || "";

        // text looks something like this: "sensor.xyz,5;sensor.abc,22" 
        // We want to convert to this: [ ["sensor.xyz", "5"], ["sensor.abc", "22"] ]
        var db = db_text.split(';');
        
        var db_length = db.length;
        for (var i = 0; i < db_length; ++i) {
            db[i] = db[i].split(',');
        }
    }
    
    
    function getAlert(entity_id) {
        for (var j = 0; j < battery_alerts_length; ++j) {
            var battery_alert = battery_alerts[j];
            if (battery_alert[0] == entity_id) {
                return battery_alert;
            }
        }
    }
    
    function getAlertIndex(entity_id) {
        for (var j = 0; j < battery_alerts_length; ++j) {
            if (battery_alert[j][0] == entity_id) {
                return j;
            }
        }
        
        return -1;
    }
    
    function insertNewAlert(entity_id, level) {
        battery_alerts_length = battery_alerts.unshift([entity_id, level]);
    }
    
    function deleteAlert(entity_id) {
        var index = getAlertIndex(entity_id);
        if (index != -1) {
            battery_alerts.splice(index, 1);
            --battery_alerts_length;
        }
    }
    
    var battery_alerts = getDb("battery_alerts");
    
    // We now loop over entire ha states array looking for attributes.battery_level
    var ha_states = hass.homeAssistant.states;
    var ha_states_len = ha_states.length;
    for (i = 0; i < ha_states_len; ++i) {
        var item = ha_states[i];
        if (item.attributes.battery_level) {
            var level = item.attributes.battery_level;
            if (level !== undefined) {
                batttery_alert = getAlert(item.entity_id);
                if (level <= 10) {
                    if (!batttery_alert) {
                        insertNewAlert(item.entity_id, level);
                        needToNotify = true;
                    } else {
                        var levelOld = batttery_alert[1];
                        batttery_alert[1] = level;
                        if (level != levelOld && ((levelOld > 5 && level <= 5) || level === 0 )) {
                            needToNotify = true;
                        }
                    }
                } else if (batttery_alert && level > 20 ) {
                    // Battery no longer low
                    deleteAlert(item.entity_id);
                }
            }
        }
    }
    
    if (needToNotify) {
        var info = hass.homeAssistant.states[msg.topic];
        msg.payload = "Alert: " + info.attributes.friendly_name + " (" + 
          info.state + info.attributes.unit_of_measurement + ") is high!";
        return msg;
    }
    