// File message

if (!msg.dbName) {
    msg.dbName = "publish"
    return [null, null, msg];
} 

var type = "";
if (msg.speak_to !== undefined && msg.notify_to !== undefined) {
    type = "am";
} else if (msg.speak_to !== undefined ) {
    type = "a";
} else if (msg.notify_to !== undefined) {
    type = "m";
}


var msgDb;

if (msg.db) {
    var today = new Date();
    var date = today.getFullYear()+'-'+(today.getMonth()+1)+'-'+today.getDate();
    var time = today.getHours() + ":" + today.getMinutes() + ":" + today.getSeconds();
    var dateTime = date + ' ' + time;
    
    var a_sent = global.get("homeassistant").homeAssistant.states["input_boolean.do_not_disturb"].state == "off";
   
    msgDb = { dbName: msg.dbName };
    msg.db.insert([type, dateTime, msg.src, msg.payload, a_sent]);
    msg.db.prune(15);
    msgDb.dbText = msg.db.close();
    msg.db = null;
}

if (type === "am") {
    return [msg, msg, null, msgDb];
} else if (type === "a") {
    return [msg, null, null, msgDb];
} else if (type === "m") {
    return [null, msg, null, msgDb];
} else {
    return [null, null, null, msgDb];
}
