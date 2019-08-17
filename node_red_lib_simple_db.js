
    hass = global.get("homeassistant");

    
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

            if (this.data === undefined) {
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

        insert(array, before = 0) {
            this.data.splice(before, 0, array);
        }
        
        add(array) {
            this.data[this.data.length] = array;
        }
        
        delete(key) {
            var index = this.getIndex(key);
            if (index != -1) {
                this.data.splice(index, 1);
            }
        }
        
        prune(max) {
            if (this.data.length > max) {
                this.data.length = max;
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
            return encodeURIComponent(JSON.stringify({ "data": this.data }));
        }

    }
    
    msg.db = new db(msg.dbName);
    
    return msg;

