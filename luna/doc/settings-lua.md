## _settings

### _settings.del

Deletes the specified settings.

Examples:

    settings = require("settings")
    settings.del("wifi.ssid")


### _settings.get

Gets the value of one or more settings.

Examples:

    settings = require("settings")
    ssid = settings.get("wifi.ssid")


### _settings.set

Sets the value of one or more settings.

Examples:

    settings = require("settings")

    settings.set("wifi.ssid", "Dagobah")

    settings.set({["wifi.ssid"]     = "Dagobah",
                  ["wifi.password"] = "12345678"})


