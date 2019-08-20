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
    //msgDb.dbText = "%7B%22data%22%3A%5B%5B%22a%22%2C%222019-8-15%2013%3A1%3A59%22%2C%22refridge%22%2C%22Hello%20world!%22%2Ctrue%5D%5D%7D"; // msg.db.close();
     // msgDb.dbText =                 "abcertyuiopasdfghjklzxcvbnmqwertyuiqwertyuiopasdfghjklzxcvbnmqwertyuiopasdfertyuiopasdfghjklzxcvbnmqwertyui"; // msg.db.close();
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
