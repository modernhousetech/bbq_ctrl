

hass = global.get("homeassistant");

try {
    
    if (hass.homeAssistant.states["input_boolean.battery_alerts_disable"].state == 'on') {
        return null;
    }
    
    
    // Simple "database" object. Array based, persisted as json
    class db {
        constructor(name) {
            this.name = name;
            if (name !== undefined && name !== null && name !== "") {
                var db_text = hass.homeAssistant.states["input_text." + name + "_db"].state || "";
                if (db_text.length !== 0) {
                    try {
                        var obj = JSON.parse(decodeURIComponent(db_text));
                        if (obj.data) {
                            this.data = obj.data;
                        }
                    } catch (e) {
                        
                    }
                }
            }

            if (this.data == undefined) {
                this.data = [];
            }
        }
        
        getIndex(key) {
            var data_length = this.data.length;
            for (var j = 0; j < data_length; ++j) {
                if (this.data[j][0] == key) {
                    return j;
                }
            }
            return -1;
        }

        get(key) {
            var index = this.getIndex(key);
            return index != -1 ? this.data[index] : undefined;
        }

        insertAtBeg(array) {
            this.data.unshift(array);
        }
        
        insertAtEnd(array) {
            this.data[this.data.length] = array;
        }
        
        delete(key) {
            var index = this.getIndex(key);
            if (index != -1) {
                this.data.splice(index, 1);
            }
        }
        
        count() {
            return this.data.length;
        }

        iterate(func) {
            var data_length = this.data.length;
            for (var i = 0; i < data_length; ++i) {
                var rc = func(this.data[i]);
                if (rc === -1 ) {
                    // -1 means delete entry
                    this.data.splice(i, 1);
                    --i;
                    --data_length;
                } else if (rc === 1) {
                    break;
                }
            }

        }

        iterate_reverse(func) {

            var data_length = this.data.length;
            for (var i = 0; i < data_length; ++i) {
                var rc = func(this.data[i]);
                if (rc === -1 ) {
                    // -1 means delete entry
                    this.data.splice(i, 1);
                    --data_length;
                } else if (rc === 1) {
                    break;
                }
            }

        }

        // Closes database and returns json for user to persist. While we can
        // read the data from an ha input_text, we can't easily write it out
        // to the same place because writing requires a different node.
        close() {
            return JSON.stringify({ "data": this.data });
        }

    }

    var i;
    var alertLevel1 = parseInt(hass.homeAssistant.states["input_number.battery_alerts_level1"].state) || 20;
    var alertLevel2 = parseInt(hass.homeAssistant.states["input_number.battery_alerts_level2"].state) || 10;

    var needToNotify = false;
    var alerts;
    
    try {
        alerts = new db("battery_alerts");
    } catch(error) {
        alerts = new db();
    }
    
    // We now loop over entire ha states array looking for attributes.battery_level
    // We notify once when a battery level first goes lower than level1
    // We notify once again when same entity's battery level goes lower than level2
    //      and once again when the level goes to zero.
    // Whenever we notify we list all devices that need new batteries.
    var ha_states = hass.homeAssistant.states;
    var ha_states_len = ha_states.length;
    
    Object.keys(hass.homeAssistant.states).forEach(key => {

        var item = hass.homeAssistant.states[key];
        if (item.attributes.battery_level && 
            // Reject entities that are just duplicate the main device
            !(
                item.entity_id.includes("xmi_humidity") ||
                item.entity_id.includes("xmi_pressure") ||
                item.entity_id.includes("bed_activity_xmi_vib") ||
                item.entity_id.includes("angle_xmi_vib") ||
                item.entity_id.includes("coordination_xmi_vib") )
            ) {
            var level = item.attributes.battery_level;
            if (level !== undefined) {
                batttery_alert = alerts.get(item.entity_id);
                // Is the level low?
                if (level <= alertLevel1) {
                    // Yes,
                    // Already in low level list?
                    if (!batttery_alert) {
                        // No, new alert
                        alerts.insertAtEnd([item.entity_id, level]);
                        needToNotify = true;
                    } else {
                        // Yes, already in list, need to update and perhaps re-notify
                        var levelOld = batttery_alert[1];
                        batttery_alert[1] = level;
                        // Need to notify again?
                        // Is new level below lowest level and was above at last check?
                        // OR now at zero?
                        if (level != levelOld && ((level <= alertLevel2 && levelOld > alertLevel2 ) || level === 0 )) {
                            needToNotify = true;
                        }
                    }
                } else if (batttery_alert && level > 20 ) {
                    // Battery no longer low
                    alerts.delete(item.entity_id);
                }
            }
        }
    });        
   
    // We now need to check our low battery list for batteries that have 
    // gone completely dead. We also need to maintain the list by
    // removing entries that are no longer in system.

    alerts.iterate_reverse(function (item) {
        var entity = hass.homeAssistant.states[item[0]];
        
        if (!entity) {
            // Item not in ha states list -- must not be in ha -- delete
            return -1;
        } else if (!isNaN(item[1]) && isNaN( entity.state) ) {
            item[1] = entity.state;
            needToNotify = true;
        }
    });

    if (needToNotify) {
        // Iterate over low battery items again. We can't combine iterations because
        // needToNotify may not be true the first time.
        var notice = "Alert - Batteries (" + alerts.count() + ") need replacement:";
        alerts.iterate_reverse(function (item) {
        
            var entity = hass.homeAssistant.states[item[0]];
            if (!entity) {
                // This entity_id is no longer active in ha
                // We now do this in first iteration above, but can't hurt
                // to do it here.
                return -1;
            } else {
                // What kind of battery?
                // cr1632
                var battery_type = "unknown";
                if (entity.entity_id.includes("xmi_dws")) {
                    battery_type = "CR1632";
                } else if (entity.entity_id.includes("xmi_btn") ||
                entity.entity_id.includes("xmi_temp") || 
                entity.entity_id.includes("xmi_vib") || 
                entity.entity_id.includes("xmi_water_detect") ) {
                    battery_type = "CR2032";
                }
                notice += "\\n- " + entity.attributes.friendly_name + " (";
                if (isNaN(item[1])) {
                    notice += "battery dead?";
                } else {
                    notice += + item[1] + "%";
                }
            }
            notice += ") - uses: " + battery_type;
        });

        msg = { payload: notice, 
        
        // Send to all phones 
        notify_to: ""
        // No audio
        //msg.speak_to = "";
        }
    } else {
        // No notification
        msg = null;
    }

    return [msg, { payload: encodeURIComponent(alerts.close()) } ];
    
} catch(error) {
    hass.homeAssistant.states["input_number.battery_alerts_failures"].state
    //msg.payload = "Error running battery level management";
    return { payload: "Error running battery level management", notify_to: ""};
}

